#pragma once

#include <filesystem>
#include <map>
#include <mutex>
#include <string>

class user;

namespace fs = std::filesystem;

/* singleton for handling users */
class user_handler {
public:

	static user_handler& getInstance() {
		static user_handler instance;
		return instance;
	};

	user_handler(user_handler const&) = delete;
	user_handler(user_handler&&) = delete;
	user_handler& operator=(user_handler const&) = delete;
	user_handler& operator=(user_handler &&) = delete;

	void setSpoolDir(fs::path p) { this->spool_dir = p; };
	fs::path getSpoolDir() { return this->spool_dir; };

	user* getUser(std::string name);
	user* getOrCreateUser(std::string name);

	void saveAll();

protected:

	user_handler();
	~user_handler();

	fs::path spool_dir;
	std::map<std::string, user*> users;

	std::mutex m_user;
	
};