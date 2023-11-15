#include "user_handler.h"
#include "user.h"

#include <filesystem>
#include <string>

user_handler::user_handler() : m_user()
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
		this->m_user.lock(); // avoid race condition of two threads creating the same user
		if (!fs::exists(this->spool_dir/"users"/(name+".json")))
			return nullptr;

		this->users.insert({name, new user(this->spool_dir/"users"/(name+".json"))});
		this->m_user.unlock();
	}
	return (*this->users.find(name)).second; 
}

user* user_handler::getOrCreateUser(std::string name)
{
	if (this->users.find(name) == this->users.end()) {
		this->m_user.lock();
		this->users.insert({name, fs::exists(this->spool_dir/"users"/(name+".json")) ?
			new user(this->spool_dir/"users"/(name+".json")) :
			new user(name, this->spool_dir/"users")});
		this->m_user.unlock();
	}
	return this->users[name]; 
}

void user_handler::saveAll()
{
	// will only be called from main
	for ( auto& user : this->users ) {
		user.second->saveToFile();
	}
}