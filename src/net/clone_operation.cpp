#include "clone_operation.h"
#include "pktline_codec.h"
#include "packfile_parser.h"
#include "delta_resolver.h"
#include "../workdir/checkout.h"
#include <iostream>
#include <fstream>

CloneOperation::CloneOperation(std::string repoUrl, std::filesystem::path targetDir)//onstructor asking for repourl and local directorty
    : repoUrl_(std::move(repoUrl)), targetDir_(std::move(targetDir)) {}

CloneOperation::DiscoveryResult CloneOperation::discoverRefs() {
    std::string discoveryUrl = repoUrl_ + "/info/refs?service=git-upload-pack";
    HttpResponse resp = http_.get(discoveryUrl, {"Git-Protocol: version=0"});//uses this protocol
    if (resp.statusCode != 200) {
        throw std::runtime_error("Discovery failed, HTTP " + std::to_string(resp.statusCode));
    }

    auto lines = PktLineCodec::parse(resp.body);//here  take raw response and convert into protocol msg
    DiscoveryResult result;

    for (const auto& line : lines) {//  loops  for parsing the reference
        if (line.isFlush) continue;
        if (line.data.rfind("# service=", 0) == 0) continue;

        std::string payload = line.data;
        if (!payload.empty() && payload.back() == '\n') payload.pop_back();

        std::string capabilities;
        size_t nulPos = payload.find('\0');
        if (nulPos != std::string::npos) {
            capabilities = payload.substr(nulPos + 1);
            payload = payload.substr(0, nulPos);
        }

        size_t spacePos = payload.find(' ');
        if (spacePos == std::string::npos) continue;

        std::string sha = payload.substr(0, spacePos);
        std::string refName = payload.substr(spacePos + 1);
        if (sha.size() != 40 || sha.find_first_not_of("0123456789abcdef") != std::string::npos) {
            std::cerr << "warning: ignoring ref with malformed object id\n";
            continue;
        }
        result.refs.emplace_back(sha, refName);//store references here also detect ref name head 
        if (refName == "HEAD") result.headSha = sha;

        if (!capabilities.empty()) {
            size_t symPos = capabilities.find("symref=HEAD:");//set branch reference head
            if (symPos != std::string::npos) {
                size_t start = symPos + std::string("symref=HEAD:").size();
                size_t end = capabilities.find(' ', start);
                result.defaultBranch = capabilities.substr(start, end == std::string::npos ? std::string::npos : end - start);
            }
        }
    }

    std::cerr << "Discovered " << result.refs.size() << " refs:\n";
    for (const auto& [sha, name] : result.refs) std::cerr << "  " << sha << " " << name << "\n";
    std::cerr << "HEAD sha: " << result.headSha << "\n";
    std::cerr << "Default branch: " << (result.defaultBranch.empty() ? "(unknown)" : result.defaultBranch) << "\n";

    return result;
}
//asking server for object files
std::string CloneOperation::fetchPackfile(const std::string& wantSha)//asking for objects {
    std::string uploadPackUrl = repoUrl_ + "/git-upload-pack";
    std::string requestBody;
    requestBody += PktLineCodec::format("want " + wantSha + " side-band-64k agent=synk/0.1\n");
    requestBody += PktLineCodec::format("");   // flush-pkt
    requestBody += PktLineCodec::format("done\n");

    HttpResponse resp = http_.post(uploadPackUrl, requestBody,
        {"Content-Type: application/x-git-upload-pack-request",
         "Accept: application/x-git-upload-pack-result"});

    if (resp.statusCode != 200) {
        throw std::runtime_error("upload-pack request failed, HTTP " + std::to_string(resp.statusCode));
    }
    std::cerr << "upload-pack response size: " << resp.body.size() << " bytes\n";
    return resp.body;
}

std::string CloneOperation::extractPackData(const std::string& rawResponse) {//exteaction of packfile
    std::vector<PktLine> packLines;
    try {
        packLines = PktLineCodec::parse(rawResponse);
    } catch (const std::exception& e) {
        std::cerr << "warning: response is not pkt-line framed (" << e.what() << "), will try raw fallback\n";
    }

    std::string packfileData;
    bool sawSideband = false;

    for (const auto& line : packLines) {
        if (line.isFlush || line.data.empty()) continue;
        unsigned char channel = static_cast<unsigned char>(line.data[0]);
        if (channel == 1 || channel == 2 || channel == 3) {
            sawSideband = true;
            if (channel == 1) {//take data
                packfileData.append(line.data, 1, std::string::npos);
            } else if (channel == 2) {
                std::cerr << "remote: " << line.data.substr(1);
            } else {
                throw std::runtime_error("fatal (remote): " + line.data.substr(1));//treat as error
            }
        }
    }

    if (!sawSideband) {
        std::cerr << "warning: no side-band detected, attempting raw fallback\n";
        size_t packStart = rawResponse.find("PACK");
        if (packStart == std::string::npos) {
            throw std::runtime_error("could not locate PACK signature in response");
        }
        packfileData = rawResponse.substr(packStart);
    }

    std::cerr << "Extracted packfile: " << packfileData.size() << " bytes\n";
    return packfileData;
}

void CloneOperation::run() {
    DiscoveryResult discovery = discoverRefs();

    if (discovery.headSha.empty() || discovery.headSha == std::string(40, '0')) {
        std::cerr << "warning: You appear to have cloned an empty repository.\n";
        std::filesystem::create_directories(targetDir_);//creating target file drectories
        return;
    }

    // --- M2: negotiate and fetch the packfile ---
    std::string rawResponse = fetchPackfile(discovery.headSha);
    std::string packfileData = extractPackData(rawResponse);

    std::filesystem::create_directories(targetDir_);
    {
        std::ofstream packOut(targetDir_ / "downloaded.pack", std::ios::binary);
        packOut.write(packfileData.data(), packfileData.size());
    }
    std::cerr << "Saved pack to " << (targetDir_ / "downloaded.pack").string() << "\n";

    // Repository::init gives us an ObjectStore/RefStore rooted at targetDir_ --
    // no process-wide chdir needed, unlike the original implementation.
    Repository repo = Repository::init(targetDir_);

    // --- M3: parse pack into individual objects (deltas not yet resolved) ---
    uint32_t headerCount = 0;
    PackfileParser parser(packfileData);
    std::vector<PackObject> packObjects = parser.parse(headerCount);
    std::cerr << "Pack header says " << headerCount << " objects, parsed " << packObjects.size() << "\n";

    // --- M4/M5: resolve delta chains of arbitrary depth ---
    std::vector<ResolvedObject> resolvedObjects = DeltaResolver::resolve(packObjects);

    int written = 0;
    for (const auto& obj : resolvedObjects) {
        repo.objects().writeRaw(packObjTypeName(obj.type), obj.data);
        written++;
    }
    std::cerr << "Wrote " << written << " objects (all delta chains resolved)\n";

    // --- M6: write refs + HEAD ---
    std::string branchName = "main";
    if (discovery.defaultBranch.rfind("refs/heads/", 0) == 0) {
        branchName = discovery.defaultBranch.substr(std::string("refs/heads/").size());
    }
    ObjectId headId = ObjectId::fromHex(discovery.headSha);
    repo.refs().writeRef("refs/heads/" + branchName, headId);
    repo.refs().setHeadToBranch(branchName);
    std::cerr << "Wrote ref refs/heads/" << branchName << " -> " << discovery.headSha << "\n";

    // --- M7: checkout HEAD commit's tree into the working directory ---
    Checkout checkout(repo.objects());
    checkout.checkoutCommit(headId, targetDir_);
    std::cerr << "Checked out working directory from HEAD commit " << discovery.headSha << "\n";
}
//this module basically first discover the remote references and identifies the head commit
//then it communicates with server with http
//receive the packfile/
//extract  packline resolve the delta objects and store data locally
//finallt set local branch and does transverse root tree also settinf repos references