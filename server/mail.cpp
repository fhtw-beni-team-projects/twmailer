#include "mail.h"
#include <mutex>

mail::mail(std::string filename, std::string subject) :
	filename(filename),
	timestamp(std::time(NULL)),
	subject(subject),
	deleted(false),
	m_file()
{}

mail::mail(std::string filename, int64_t timestamp, std::string subject) :
	filename(filename),
	timestamp(timestamp),
	subject(subject),
	deleted(false),
	m_file()
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
	std::remove((user_handler::getInstance().getSpoolDir()/"objects"/fs::path(this->filename.insert(2, "/"))).c_str());

	this->filename = "";
}

json mail::mailToJson()
{
	json jsonfile;

	jsonfile["id"] = this->id;
	jsonfile["timestamp"] = this->timestamp;
	jsonfile["sender"] = this->sender;
	jsonfile["recipients"] = this->recipients;
	jsonfile["subject"] = this->subject;
	jsonfile["filename"] = this->filename;
	jsonfile["deleted"] = this->deleted;

	return jsonfile;
}