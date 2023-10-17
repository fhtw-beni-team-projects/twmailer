#include "user.h"
#include "user_handler.h"
#include "mail.h"

#include <cstdio>
#include <fstream>
#include <ostream>
#include <string>
#include <vector>

using json = nlohmann::json;

user::user(fs::path user_data_json)
{
	std::ifstream ifs(user_data_json);
	json user = json::parse(ifs);

	this->name = user["name"];
	for ( auto& mail_json : user["mails"]["received"] ) {
		mail* mail = new struct mail(
			mail_json["filename"],
			mail_json["timestamp"],
			mail_json["subject"]
		);
		mail->id = mail_json["id"];
		mail->sender = mail_json["sender"];
		mail->recipients = mail_json["recipients"].get<std::vector<std::string>>();
		
		this->inbox.insert(mail);
	}
}

user::user(std::string name, fs::path user_dir)
	: name(name)
{
	json user;
	user["mails"]["sent"] = json::object();
	user["mails"]["received"] = json::object();
	user["name"] = name;

	std::ofstream ofs(user_dir/(name+".json"));
	ofs << user;
	this->user_data = user;
	this->file_location = user_dir/(name+".json");
}

user::~user() {

}

void user::addMail(mail* mail) 
{
	mail->id = this->inbox.size();

	this->inbox.insert(mail);
	this->user_data["mails"]["received"][std::to_string(mail->id)] = mail->mailToJson();

	printf("%s\n", this->user_data.dump().c_str());
}

void user::sendMail(mail* mail, std::vector<std::string> recipients) 
{
	user_handler* user_handler = user_handler::getInstance();
	std::vector<user*> users;
	for ( auto& name : recipients) {
		// TODO: error handling for non existing user
		users.push_back(user_handler->getUser(name));
	}

	mail->sender = this->name;
	mail->recipients = recipients;

	this->user_data["mails"]["sent"][std::to_string(mail->id)] = mail->mailToJson();

	for ( auto& user : users ) {
		user->addMail(mail);
	}
}

mail* user::getMail(u_int id) 
{
	maillist::iterator it = std::find_if(this->inbox.begin(), this->inbox.end(), [id](auto& i){ return (*i)(id); });
	return it == this->inbox.end() ? nullptr : (*it)->filename.empty() ? nullptr : *it;
}

void user::saveToFile()
{
	std::ofstream ofs(this->file_location);
	ofs << this->user_data;
}