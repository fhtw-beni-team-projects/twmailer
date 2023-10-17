#pragma once

#include "user.h"

#include <filesystem>
#include <map>
#include <string>

namespace fs = std::filesystem;

/* singleton for handling users */
class user_handler {
public:

	user_handler(const user_handler& obj) = delete;

	static user_handler* getInstance() {
		if (instancePtr == nullptr) {
			instancePtr = new user_handler();
			return instancePtr;
		} else {
			return instancePtr;
		}
	};
	~user_handler();

	void setSpoolDir(fs::path p) { this->spool_dir = p; };
	fs::path getSpoolDir() { return this->spool_dir; };
	user* getUser(std::string name);

	void saveAll();

private:

	static user_handler* instancePtr;
	user_handler();

	fs::path spool_dir;
	std::map<std::string, user*> users;
	
};