#ifndef COMMAND_H
#define COMMAND_H

#include <string>

int git_init();
int cat_file(const std::string &sha1, const std::string &flag);
int hash_object(const std::string &file_path, bool write);
#endif
