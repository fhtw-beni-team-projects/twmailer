#include "user_handler.h"
#include "user.h"

#include <filesystem>
#include <string>

user_handler::user_handler()
{
	for (const auto& entry : fs::directory_iterator()) {
		if (entry.path().extension() == ".json") {
			this->users.insert(std::pair<std::string, user*>(fs::path(entry.path()).replace_extension(), new user(entry)));
		}
	}
}

user_handler::~user_handler()
{
	//
}

user* user_handler::getUser(std::string name)
{
	if (this->users.find(name) == this->users.end()) {
		if (!fs::exists(this->spool_dir/"users"/(name+".json")))
			return nullptr;

		this->users[name] = new user(this->spool_dir/"users"/(name+".json"));
	}
	return this->users[name]; 
}

user* user_handler::getOrCreateUser(std::string name)
{
	if (this->users.find(name) == this->users.end()) {
		this->users[name] = fs::exists(this->spool_dir/"users"/(name+".json")) ?
			new user(this->spool_dir/"users"/(name+".json")) :
			new user(name, this->spool_dir/"users");
	}
	return this->users[name]; 
}

void user_handler::saveAll()
{
	for ( auto& user : this->users ) {
		user.second->saveToFile();
	}
}