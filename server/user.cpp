#include "user.h"
#include "user_handler.h"
#include "mail.h"

#include <cstdio>
#include <fstream>
#include <mutex>
#include <ostream>
#include <string>
#include <vector>
#include <map>

using json = nlohmann::json;

user::user(fs::path user_data_json) : m()
{
	std::ifstream ifs(user_data_json);
	json user_data = json::parse(ifs);

	this->name = user_data["name"];
	for ( auto& mail_json : user_data["mails"] ) {
		mail* mail = new struct mail(
			mail_json["filename"],
			mail_json["timestamp"],
			mail_json["subject"],
			mail_json["sender"]
		);
		mail->id = mail_json["id"];
		mail->recipient = mail_json["recipient"];
		mail->deleted = mail_json["deleted"];
		
		this->inbox.insert(mail);
	}

	this->user_data = user_data;
	this->file_location = user_data_json;
}

user::user(std::string name, fs::path user_dir)
	: name(name),
	  m()
{
	json user;
	user["mails"] = json::object();
	user["name"] = name;

	std::ofstream ofs(user_dir/(name+".json"));
	ofs << user;
	this->user_data = user;
	this->file_location = user_dir/(name+".json");
}

user::~user() {
	for (auto& mail : this->inbox) {
		delete(mail);
	}
}

void user::addMail(mail* mail) 
{
	std::unique_lock<std::shared_mutex> lock(this->m);

	mail->id = this->inbox.size();
	mail->recipient = this->name;

	this->inbox.insert(mail);
	this->user_data["mails"][std::to_string(mail->id)] = mail->mailToJson();
}

mail* user::getMail(u_int id) 
{
	std::shared_lock<std::shared_mutex> lock(this->m);
	maillist::iterator it = std::find_if(this->inbox.begin(), this->inbox.end(), [id](auto& i){ return (*i)(id); });
	return it == this->inbox.end() ? nullptr : (*it)->filename.empty() ? nullptr : *it; // TODO: potentially not thread safe, research if iterator points to 
}

bool user::delMail(u_int id) 
{
	std::unique_lock<std::shared_mutex> lock(this->m);

	maillist::iterator it = std::find_if(this->inbox.begin(), this->inbox.end(), [id](auto& i){ return (*i)(id); });

	bool success = true;

	if (it == this->inbox.end() ||
		(*it)->deleted)
		return false;

	if (!(*it)->filename.empty()) {
		std::unique_lock<std::mutex> lock((*it)->m_file);
		success = fs::remove(user_handler::getInstance().getSpoolDir()/"messages"/(*it)->filename);
	}

	if (success) {
		this->user_data["mails"][std::to_string((*it)->id)]["subject"] = "";
		this->user_data["mails"][std::to_string((*it)->id)]["filename"] = "";
		this->user_data["mails"][std::to_string((*it)->id)]["deleted"] = true;
		(*it)->deleted = true; // other info will be deleted on shutdown
	}

	return success;
}

void user::saveToFile()
{
	std::shared_lock<std::shared_mutex> lock(this->m);

	std::ofstream ofs(this->file_location, std::ofstream::out | std::ofstream::trunc);
	ofs << this->user_data.dump();
}