#include <iostream>
#include "boost\asio.hpp"
#include "boost\bind.hpp"
#include <boost\shared_ptr.hpp>
#include "boost\regex.hpp"
#include "boost\asio\deadline_timer.hpp"
#include "boost\date_time\posix_time\posix_time.hpp"
#include <list>
#include <stdlib.h>
#define OK 0
#define FAIL -1
using namespace boost::asio;
typedef boost::shared_ptr<ip::tcp::socket> socket_ptr;

class Upnp{

	struct mapping_desc{
		std::string external_ip; 
		std::string external_port; 
		std::string internal_ip;
		std::string internal_port;
		std::string protocol;
		std::string port_forwading_description;
		friend bool operator==(const mapping_desc& right,const mapping_desc &left)
		{
			return (right.external_port == left.external_port &&
				right.external_ip == left.external_ip &&
				right.protocol == left.protocol) ? 1 : 0;
		}
	};
	io_service service;
	boost::asio::ip::address rout_addr;
	std::list<mapping_desc> mapping_list;
	std::list<mapping_desc> my_mapping_list;
	size_t ReadComplete(char* buff_, const boost::system::error_code & err, size_t bytes) {
		if (err)
			return 0;
		char crlf[] = { '\r', '\n', '\r', '\n' };
		bool found = std::find_end(buff_, buff_ + bytes, crlf, crlf + 4) < buff_ + bytes;
		return found ? 0 : 1;
	}
	size_t ReadComplete2(int length, const boost::system::error_code & err, size_t bytes) {
		if (err)
			return 0;
		if (bytes == length)
			return 0;
		return 1;
	}
	size_t ReadComplete3(char* buff_, const boost::system::error_code & err, size_t bytes) {
		if (err)
			return 0;
		char crlf[] = { '\r', '\n' };
		bool found = std::find_end(buff_, buff_ + bytes, crlf, crlf + 2) < buff_ + bytes;
		return found ? 0 : 1;
	}
	int SendReqToRout(socket_ptr sock_ptr, const std::string request, std::string *responce)
	{
		int bytes_sent = sock_ptr->send(buffer(request));
		char buff_[1024];
		size_t bytes_recived = read(*sock_ptr, buffer(buff_), boost::bind(&Upnp::ReadComplete, this,buff_, _1, _2));
		std::string responce_header(buff_, bytes_recived);
		int Len_pos;
		if (responce_header.find("HTTP/1.1 200 OK") >= bytes_recived)
		{
			*responce = responce_header;
			return FAIL;
		}
		else if ((Len_pos = responce_header.find("Content-Length: ")) < bytes_recived)
		{
			Len_pos += 16;
			int end_line = responce_header.find("\r\n", Len_pos);
			int Len_len = end_line - Len_pos;
			std::string Length = responce_header.substr(Len_pos, Len_len);
			std::stringstream ss;
			ss << Length;
			int length;
			ss >> length;
			size_t bytes_recived = read(*sock_ptr, buffer(buff_), boost::bind(&Upnp::ReadComplete2, this, length, _1, _2));
			*responce = std::string(buff_, bytes_recived);
		}
		else if (responce_header.find("Transfer-Encoding: chunked\r\n") < bytes_recived)
		{
			std::string result;
			while (1)
			{
				int chunk_len_len = read(*sock_ptr, buffer(buff_), boost::bind(&Upnp::ReadComplete3, this, buff_, _1, _2));
				chunk_len_len -= 2;
				char* char_chunk_len = new char[chunk_len_len + 1];
				for (int i = 0; buff_[i] != '\r'; i++)
					char_chunk_len[i] = buff_[i];
				char_chunk_len[chunk_len_len] = '\0';
				char *next;
				long int chunk_len = strtol(char_chunk_len, &next, 16);
				free(char_chunk_len);
				if (chunk_len == 0)
				{
					break;
				}
				chunk_len += 2;
				char c[1];
				char* chunk = new char[chunk_len + 1];
				int j = 0;
				while (read(*sock_ptr, buffer(c)) > 0)
				{
					chunk[j] = c[0];
					j++;
					if (j == chunk_len)
						break;
				}
				chunk[chunk_len] = '\0';
				result.append(chunk);
				free(chunk);
			}
			*responce = result;
		}
		return OK;

	}
public:
	int SearchForRouter(std::string* search_result)
	{
		boost::asio::ip::tcp::resolver resolver(service);
		boost::asio::ip::tcp::resolver::query query(boost::asio::ip::host_name(), "");
		boost::asio::ip::tcp::resolver::iterator it = resolver.resolve(query);
		boost::asio::ip::tcp::endpoint endpoint = *it;
		boost::asio::ip::address addr;
		while (it != boost::asio::ip::tcp::resolver::iterator())
		{
			addr = (it++)->endpoint().address();
			if (addr.is_v4())
			{
				break;
			}
		}
		ip::udp::endpoint ep(addr, 0);
		ip::udp::socket multicast_socket(service, ep.protocol());
		multicast_socket.bind(ep);
		std::string search_req("M-SEARCH * HTTP/1.1\r\nHost:239.255.255.250:1900\r\nST:urn:schemas-upnp-org:device:InternetGatewayDevice:1\r\nMan:\"ssdp:discover\"\r\nMX:3\r\n\r\n");
		ip::udp::endpoint multicast_address(ip::address::from_string("239.255.255.250"), 1900);
		int bytes_send = multicast_socket.send_to(buffer(search_req), multicast_address);
		ip::udp::endpoint sender_ep;
		char buff[1024];
		int bytes_recieved = multicast_socket.receive_from(buffer(buff), sender_ep);
		buff[bytes_recieved] = '\0';
		*search_result = buff;
		multicast_socket.close();
		return OK;
	}
	int FindRouterLocation(const std::string Router, std::string* location)
	{
		boost::regex xRegEx("192.168(.(\\d){1,3}){2}:\\d+");
		boost::smatch xResults;
		boost::regex_search(Router, xResults, xRegEx);
		*location = xResults[0];
		xRegEx.assign("192.168(.(\\d){1,3}){2}");
		boost::regex_search(*location, xResults, xRegEx);
		rout_addr = boost::asio::ip::address(ip::address::from_string(xResults[0]));
		return OK;
	}
	int GetDeviceDescription(const std::string location, std::string* device_description)
	{
		std::string responce;
		ip::tcp::endpoint ep(rout_addr, 80);
		socket_ptr sock_ptr(new ip::tcp::socket(service));
		sock_ptr->connect(ep);
		std::string get_request("GET http://" + location + "/DeviceDescription.xml HTTP/1.1  \r\n  HOST:192.168.1.1:80 \r\n ACCEPT-LANGUAGE: EN-en \r\n\r\n");
		if (SendReqToRout(sock_ptr, get_request, &responce) != OK)
		{
			sock_ptr->close();
			return FAIL;
		}
		sock_ptr->close();
		return OK;
	}
	int GetWanIpServiceDescrption(const std::string location, std::string* wan_ip_service_descrption)
	{
		std::string responce;
		ip::tcp::endpoint ep(rout_addr, 80);
		socket_ptr sock_ptr(new ip::tcp::socket(service));
		sock_ptr->connect(ep);
		std::string get_request("GET http://" + location + "/WanIpConn.xml HTTP/1.1  \r\n  HOST:192.168.1.1:80 \r\n ACCEPT-LANGUAGE: EN-en \r\n\r\n");
		if (SendReqToRout(sock_ptr, get_request, &responce) != OK)
		{
			sock_ptr->close();
			return FAIL;
		}
		sock_ptr->close();
		*wan_ip_service_descrption = responce;
		return OK;
	}
	int GetExternalIpAddress(const std::string location, std::string* external_ip)
	{
		std::string responce;
		ip::tcp::endpoint ep(rout_addr, 80);
		socket_ptr sock_ptr(new ip::tcp::socket(service));
		sock_ptr->connect(ep);
		std::string get_request("POST /UD/?3 HTTP/1.1\r\n"
			"Content-Type: text/xml; charset=\"utf-8\"\r\n"
			"SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#GetExternalIPAddress\"\r\n"
			"User-Agent: Mozilla/4.0 (compatible; UPnP/1.0; Windows 9x)\r\n"
			"Host: " + location + "\r\n"
			"Content-Length: 303\r\n"
			"Connection: Close\r\n"
			"Cache-Control: no-cache\r\n"
			"Pragma: no-cache\r\n\r\n"
			"<?xml version=\"1.0\"?>"
			"<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">"
			"<SOAP-ENV:Body>"
			"<m:GetExternalIPAddress xmlns:m=\"urn:schemas-upnp-org:service:WANIPConnection:1\"/>"
			"</SOAP-ENV:Body>"
			"</SOAP-ENV:Envelope>\r\n\r\n");
		if (SendReqToRout(sock_ptr, get_request, &responce) != OK)
		{
			sock_ptr->close();
			return FAIL;
		}
		sock_ptr->close();
		int i = 0;
		int begin = responce.find("<NewExternalIPAddress>") + 22;
		int end = responce.find("</NewExternalIPAddress>");
		*external_ip = responce.substr(begin, end - begin);
		sock_ptr->close();
		return OK;
	}
	int NewPortMapping(const std::string location, const std::string external_ip, const std::string external_port, const std::string internal_ip, const std::string internal_port, const std::string protocol, const std::string port_forwading_description)
	{
		std::string responce;
		ip::tcp::endpoint ep(rout_addr, 80);
		socket_ptr sock_ptr(new ip::tcp::socket(service));
		sock_ptr->connect(ep);
		std::string add_forwading_request("<?xml version=\"1.0\"?>\r\n"
			"<SOAP-ENV:Envelope\r\n xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\"\r\n SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
			"<SOAP-ENV:Body>\r\n"
			"<m:AddPortMapping xmlns:m=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n"
			"<NewRemoteHost>"
			+ external_ip +
			"</NewRemoteHost>\r\n"
			"<NewExternalPort>"
			+ external_port +
			"</NewExternalPort>\r\n"
			"<NewProtocol>"
			+ protocol +
			"</NewProtocol>\r\n"
			"<NewInternalPort>"
			+ internal_port +
			"</NewInternalPort>\r\n"
			"<NewInternalClient>"
			+ internal_ip +
			"</NewInternalClient>\r\n"
			"<NewEnabled>"
			"1"
			"</NewEnabled>\r\n"
			"<NewPortMappingDescription>"
			+ port_forwading_description +
			"</NewPortMappingDescription>\r\n"
			"<NewLeaseDuration>"
			"0"
			"</NewLeaseDuration>\r\n"
			"</m:AddPortMapping>\r\n"
			"</SOAP-ENV:Body>\r\n"
			"</SOAP-ENV:Envelope>\r\n\r\n");
		std::string request_header("POST /UD/?3 HTTP/1.1\r\n"
			"Content-Type: text/xml; charset=\"utf-8\"\r\n"
			"SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#AddPortMapping\"\r\n"
			"Host: " + location + "\r\n"
			"Content-Length: " + std::to_string(add_forwading_request.length()) + "\r\n"
			"Connection: Close\r\n"
			"Cache-Control: no-cache\r\n"
			"Pragma: no-cache\r\n\r\n");
		request_header.append(add_forwading_request);
		if (SendReqToRout(sock_ptr, request_header, &responce) != OK)
		{
			if (responce.find("HTTP/1.1 500 Internal Server Error") != -1)
			{
				int Len_pos = responce.find("Content-Length:");
				Len_pos += 16;
				int end_line = responce.find("\r\n", Len_pos);
				int Len_len = end_line - Len_pos;
				std::string Length = responce.substr(Len_pos, Len_len);
				std::stringstream ss;
				ss << Length;
				int length;
				ss >> length;
				char* error_responce = new char[length + 1];
				char c[1];
				int j = 0;
				while (read(*sock_ptr, buffer(c)) > 0)
				{
					error_responce[j] = c[0];
					j++;
					if (j == length)
						break;
				}
				error_responce[length] = '\0';
			}
			sock_ptr->close();
			return FAIL;
		}
		sock_ptr->close();
		mapping_desc m_desc;
		m_desc.external_ip = external_ip;
		m_desc.external_port = external_port;
		m_desc.protocol = protocol;
		m_desc.internal_port = internal_port;
		m_desc.internal_ip = internal_ip;
		m_desc.port_forwading_description = port_forwading_description;
		my_mapping_list.push_back(m_desc);
		mapping_list.push_back(m_desc);
		return OK;
	}
	int GetPortMappingEntry(const std::string location, const std::string external_ip, const std::string external_port, const std::string protocol)
	{
		std::string responce;
		ip::tcp::endpoint ep(rout_addr, 80);
		socket_ptr sock_ptr(new ip::tcp::socket(service));
		sock_ptr->connect(ep);
		std::string get_forwading_request("<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
			"<SOAP-ENV:Body>\r\n"
			"<m:GetSpecificPortMappingEntry xmlns:m=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n"
			"<NewRemoteHost>" + external_ip + "</NewRemoteHost>\r\n"
			"<NewExternalPort>" + external_port + "</NewExternalPort>\r\n"
			"<NewProtocol>" + protocol + "</NewProtocol>\r\n"
			"</m:GetSpecificPortMappingEntry>\r\n"
			"</SOAP-ENV:Body>\r\n"
			"</SOAP-ENV:Envelope>\r\n\r\n");
		std::string ger_forwading_request_header("POST /UD/?3 HTTP/1.1\r\n"
			"Content-Type: text/xml; charset=\"utf-8\"\r\n"
			"SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#GetSpecificPortMappingEntry\"\r\n"
			"User-Agent: Mozilla/4.0 (compatible; UPnP/1.0; Windows 9x)\r\n"
			"Host: " + location + "\r\n"
			"Content-Length: " + std::to_string(get_forwading_request.length()) + "\r\n"
			"Connection: Close\r\n"
			"Cache-Control: no-cache\r\n"
			"Pragma: no-cache\r\n\r\n");
		ger_forwading_request_header.append(get_forwading_request);
		if (SendReqToRout(sock_ptr, ger_forwading_request_header, &responce) != OK)
		{
			if (responce.find("HTTP/1.1 500 Internal Server Error") != -1)
			{
				int Len_pos = responce.find("Content-Length:");
				Len_pos += 16;
				int end_line = responce.find("\r\n", Len_pos);
				int Len_len = end_line - Len_pos;
				std::string Length = responce.substr(Len_pos, Len_len);
				std::stringstream ss;
				ss << Length;
				int length;
				ss >> length;
				char* error_responce = new char[length + 1];
				char c[1];
				int j = 0;
				while (read(*sock_ptr, buffer(c)) > 0)
				{
					error_responce[j] = c[0];
					j++;
					if (j == length)
						break;
				}
				error_responce[length] = '\0';
			}
			sock_ptr->close();
			return FAIL;
		}
		sock_ptr->close();
		return OK;
	}
	int GetGenericPortMappingEntry(const std::string location, const int port_mapping_index)
	{
		std::string responce;
		ip::tcp::endpoint ep(rout_addr, 80);
		socket_ptr sock_ptr(new ip::tcp::socket(service));
		sock_ptr->connect(ep);
		std::string get_forwading_request("<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
			"<SOAP-ENV:Body>\r\n"
			"<m:GetGenericPortMappingEntry xmlns:m=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n"
			"<NewPortMappingIndex>" + std::to_string(port_mapping_index) + "</NewPortMappingIndex>\r\n"
			"</m:GetGenericPortMappingEntry>\r\n"
			"</SOAP-ENV:Body>\r\n"
			"</SOAP-ENV:Envelope>\r\n\r\n");
		std::string ger_forwading_request_header("POST /UD/?3 HTTP/1.1\r\n"
			"Content-Type: text/xml; charset=\"utf-8\"\r\n"
			"SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#GetGenericPortMappingEntry\"\r\n"
			"User-Agent: Mozilla/4.0 (compatible; UPnP/1.0; Windows 9x)\r\n"
			"Host: " + location + "\r\n"
			"Content-Length: " + std::to_string(get_forwading_request.length()) + "\r\n"
			"Connection: Close\r\n"
			"Cache-Control: no-cache\r\n"
			"Pragma: no-cache\r\n\r\n");
		ger_forwading_request_header.append(get_forwading_request);
		if (SendReqToRout(sock_ptr, ger_forwading_request_header, &responce) != OK)
		{
			if (responce.find("HTTP/1.1 500 Internal Server Error") != -1)
			{
				int Len_pos = responce.find("Content-Length:");
				Len_pos += 16;
				int end_line = responce.find("\r\n", Len_pos);
				int Len_len = end_line - Len_pos;
				std::string Length = responce.substr(Len_pos, Len_len);
				std::stringstream ss;
				ss << Length;
				int length;
				ss >> length;
				char* error_responce = new char[length + 1];
				char c[1];
				int j = 0;
				while (read(*sock_ptr, buffer(c)) > 0)
				{
					error_responce[j] = c[0];
					j++;
					if (j == length)
						break;
				}
				error_responce[length] = '\0';
			}
			sock_ptr->close();
			return FAIL;
		}
		mapping_desc m_desc;
		boost::regex xRegEx("\\s*<NewRemoteHost>(.*)</NewRemoteHost>"
			"\\s*<NewExternalPort>(.*)</NewExternalPort>"
			"\\s*<NewProtocol>(.*)</NewProtocol>"
			"\\s*<NewInternalPort>(.*)</NewInternalPort>"
			"\\s*<NewInternalClient>(.*)</NewInternalClient>"
			"\\s*<NewEnabled>1</NewEnabled>"
			"\\s*<NewPortMappingDescription>(.*)</NewPortMappingDescription>");
		boost::smatch xResults;
		boost::regex_search(responce, xResults, xRegEx);
		m_desc.external_ip = xResults[1];
		m_desc.external_port = xResults[2];
		m_desc.protocol = xResults[3];
		m_desc.internal_port = xResults[4];
		m_desc.internal_ip = xResults[5];
		m_desc.port_forwading_description = xResults[6];
		mapping_list.push_back(m_desc);
		sock_ptr->close();
		return OK;
	}
	int DeletePortMapping(const std::string location, const std::string external_ip, const std::string external_port, const std::string protocol)
	{
		std::string responce;
		ip::tcp::endpoint ep(rout_addr, 80);
		socket_ptr sock_ptr(new ip::tcp::socket(service));
		sock_ptr->connect(ep);
		std::string delete_forwading_request("<?xml version=\"1.0\"?>\r\n"
			"<SOAP-ENV:Envelope\r\n xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\"\r\n SOAP-ENV:encodingStyle=\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n"
			"<SOAP-ENV:Body>\r\n"
			"<m:DeletePortMapping xmlns:m=\"urn:schemas-upnp-org:service:WANIPConnection:1\">\r\n"
			"<NewRemoteHost>"
			+ external_ip +
			"</NewRemoteHost>\r\n"
			"<NewExternalPort>"
			+ external_port +
			"</NewExternalPort>\r\n"
			"<NewProtocol>"
			+ protocol +
			"</NewProtocol>\r\n"
			"</m:DeletePortMapping>\r\n"
			"</SOAP-ENV:Body>\r\n"
			"</SOAP-ENV:Envelope>\r\n\r\n");
		std::string delete_forwding_request_header("POST /UD/?3 HTTP/1.1\r\n"
			"Content-Type: text/xml; charset=\"utf-8\"\r\n"
			"SOAPAction: \"urn:schemas-upnp-org:service:WANIPConnection:1#DeletePortMapping\"\r\n"
			"User-Agent: Mozilla/4.0 (compatible; UPnP/1.0; Windows 9x)\r\n"
			"Host: " + location + "\r\n"
			"Content-Length: " + std::to_string(delete_forwading_request.length()) + "\r\n"
			"Connection: Keep-Alive\r\n"
			"Cache-Control: no-cache\r\n"
			"Pragma: no-cache\r\n\r\n");
		delete_forwding_request_header.append(delete_forwading_request);
		if (SendReqToRout(sock_ptr, delete_forwding_request_header, &responce) != OK)
		{
			if (responce.find("HTTP/1.1 500 Internal Server Error") != -1)
			{
				int Len_pos = responce.find("Content-Length:");
				Len_pos += 16;
				int end_line = responce.find("\r\n", Len_pos);
				int Len_len = end_line - Len_pos;
				std::string Length = responce.substr(Len_pos, Len_len);
				std::stringstream ss;
				ss << Length;
				int length;
				ss >> length;
				char* error_responce = new char[length + 1];
				char c[1];
				int j = 0;
				while (read(*sock_ptr, buffer(c)) > 0)
				{
					error_responce[j] = c[0];
					j++;
					if (j == length)
						break;
				}
				error_responce[length] = '\0';
			}
			sock_ptr->close();
			return FAIL;
		}
		sock_ptr->close();
		mapping_desc m_desc;
		m_desc.external_ip = external_ip;
		m_desc.external_port = external_port;
		m_desc.protocol = protocol;
		my_mapping_list.remove(m_desc);
		mapping_list.remove(m_desc);
		return OK;
	}
	void SeeAllPortMappings()
	{
		for (std::list<mapping_desc>::iterator c_it = mapping_list.begin(); c_it != mapping_list.end(); c_it++)
		{
			std::cout << "Port mapping: " << c_it.operator*().port_forwading_description << std::endl;
			std::cout << "External port: " << c_it.operator*().external_port << std::endl;
			std::cout << "Protocol: " << c_it.operator*().protocol << std::endl;
			std::cout << "Internal port: " << c_it.operator*().internal_port << std::endl;
			std::cout << "Internal ip: " << c_it.operator*().internal_ip << std::endl;
			std::cout << std::endl;
		}
	}
	void SeeMyPortMapping()
	{
		for (std::list<mapping_desc>::iterator c_it = my_mapping_list.begin(); c_it != my_mapping_list.end(); c_it++)
		{
			std::cout << "Port mapping: " << c_it.operator*().port_forwading_description << std::endl;
			std::cout << "External port: " << c_it.operator*().external_port << std::endl;
			std::cout << "Protocol: " << c_it.operator*().protocol << std::endl;
			std::cout << "Internal port: " << c_it.operator*().internal_port << std::endl;
			std::cout << "Internal ip: " << c_it.operator*().internal_ip << std::endl;
			std::cout << std::endl;
		}
	}
};