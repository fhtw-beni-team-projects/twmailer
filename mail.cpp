#include "mail.h"

mail::mail(std::string filename, int64_t timestamp, std::string subject) :
	filename(filename),
	timestamp(timestamp),
	subject(subject)
{}

/*
fs::path mail::getPath() {
	if (this->filename.empty())
		return fs::path();
	return fs::path(this->filename.insert(2, "/"));
}
*/

void mail::remove()
{
	if (this->filename.empty())
		return;
	std::remove((user_handler::getInstance()->getSpoolDir()/"objects"/fs::path(this->filename.insert(2, "/"))).c_str());

	this->filename = "";
}

json mail::mailToJson()
{
	json json;

	json["id"] = this->id;
	json["timestamp"] = this->timestamp;
	json["sender"] = this->sender;
	json["recipients"] = this->recipients;
	json["subject"] = this->subject;
	json["filename"] = this->filename;

	return json;
}