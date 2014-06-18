#include <iostream>
#include "boost\asio.hpp"
#include "boost\bind.hpp"
#include <boost\shared_ptr.hpp>
#include "boost\regex.hpp"
#include <locale>
using namespace boost::asio;
#ifdef WIN32
#include <iphlpapi.h>
#pragma comment(lib,"IPHlpApi.lib")
#endif
typedef boost::shared_ptr<ip::udp::socket> socket_ptr_udp;
class Natpmp{

	struct Request_pmp{
		Request_pmp() : try_number(0) {};
		unsigned char request[12];
		int request_len, try_number;
	};
	struct Responce_pmp {
		Responce_pmp() : type(0), result_code(0), private_port(0),
		public_port(0), epoch(0), lifetime(0),
		public_address() {}

		uint16_t type, result_code, private_port, public_port;
		uint32_t epoch, lifetime;
		boost::asio::ip::address public_address;
	};

	boost::asio::ip::address rout_addr;
	io_service service;
	deadline_timer d_timer;
	socket_ptr_udp sock_ptr;
	enum{ port = 5351 };
	enum{ bufsize = 512 };
	enum{ TCP = 1, UDP = 2 };
	char buf[bufsize];
	Request_pmp request_pmp;
	Responce_pmp responce_pmp;
	ip::udp::endpoint ep_;
	boost::asio::ip::address m_public_ip_address_;

public:
	Natpmp() : d_timer(service), sock_ptr(new ip::udp::socket(service)), m_public_ip_address_(boost::asio::ip::address_v4::any()) {
#ifdef WIN32
		std::string _rout_addr;
		int err = getdefaultgateway(&_rout_addr);
		rout_addr = boost::asio::ip::address(ip::address::from_string(_rout_addr));
#else
		rout_addr = boost::asio::ip::address(ip::address::from_string("192.168.1.1"));
#endif
		boost::asio::ip::udp::endpoint ep(rout_addr, port);
		sock_ptr->async_connect(ep, boost::bind(&Natpmp::HandleAsyncConnect, this, boost::asio::placeholders::error));
		service.run();
		setlocale(LC_ALL, "Rus");
	}
	int GetExternalIPAddress()
	{
		request_pmp.request[0] = 0;
		request_pmp.request[1] = 0;
		request_pmp.request_len = 2;
		request_pmp.try_number = 1;
		SendRequest(&request_pmp);
		d_timer.expires_from_now(boost::posix_time::milliseconds(
			250 * request_pmp.try_number));

		d_timer.async_wait(boost::bind(
			&Natpmp::RetransmitPublicAdddressRequest, this, _1));
		return 0;
	}
	void NewPortMapping(uint32_t protocol,uint16_t private_port,uint16_t public_port,uint32_t lifetime = 7200)
	{
		request_pmp.request[0] = 0;
		request_pmp.request[1] = static_cast<char>(protocol);
		request_pmp.request[2] = 0;
		request_pmp.request[3] = 0;

		*(reinterpret_cast<uint16_t*>((request_pmp.request + 4))) = htons(private_port);
		*(reinterpret_cast<uint16_t*>((request_pmp.request + 6))) = htons(public_port);
		*(reinterpret_cast<uint32_t*>((request_pmp.request + 8))) = htonl(lifetime);

		request_pmp.request_len = 12;
		request_pmp.try_number = 1;
		SendRequest(&request_pmp);
		d_timer.expires_from_now(boost::posix_time::milliseconds(
			250 * request_pmp.try_number));

		d_timer.async_wait(boost::bind(
			&Natpmp::RetransmitPublicAdddressRequest, this, _1));
	}
	void DeletePortMapping(uint32_t protocol, uint16_t private_port, uint16_t public_port)
	{
		NewPortMapping(protocol, private_port, public_port, 0);
	}

	

private:
	void Natpmp::RetransmitPublicAdddressRequest(
		const boost::system::error_code & ec) {
		if (ec) {
		}
		else if (request_pmp.try_number >= 9) {

			d_timer.cancel();
		}
		else if (m_public_ip_address_ == boost::asio::ip::address_v4::any()) {
			++request_pmp.try_number;

			SendRequest(&request_pmp);


			d_timer.expires_from_now(boost::posix_time::milliseconds(
				250 * request_pmp.try_number));

			d_timer.async_wait(boost::bind(
				&Natpmp::RetransmitPublicAdddressRequest, this, _1));
		}
	}
	void SendRequest(const Request_pmp *request_pmp)
	{
		if (sock_ptr && sock_ptr->is_open()) {
			sock_ptr->async_send(buffer(request_pmp->request, request_pmp->request_len), boost::bind(
				&Natpmp::HandleSend,
				this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
		}
	}
	void Natpmp::HandleSend(const boost::system::error_code & ec,
		std::size_t)
	{
		if (ec == boost::asio::error::operation_aborted) {

		}
		if (!ec)
			sock_ptr->async_receive_from(
				boost::asio::buffer(buf, sizeof(buf)),
				ep_,
				boost::bind(
				&Natpmp::HandleReceiveFrom,
				this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
		
	}
	void Natpmp::HandleReceiveFrom(const boost::system::error_code & ec,
		std::size_t bytes) {
		if (ec == boost::asio::error::operation_aborted) {

		}
		if(!ec) {
			HandleResponse(buf, bytes);

			sock_ptr->async_receive_from(
				boost::asio::buffer(buf, sizeof(buf)),
				ep_,
				boost::bind(
				&Natpmp::HandleReceiveFrom,
				this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred));
		}
	}
	void Natpmp::HandleResponse(const char * buf, std::size_t) {
		uint32_t opcode = 0;


		if (ep_.address() == rout_addr) {
			responce_pmp.result_code = ntohs(*(reinterpret_cast<const uint16_t*>(buf + 2)));

			responce_pmp.epoch = ntohl(*(reinterpret_cast<const uint32_t*>(buf + 4)));

			if (buf[0] != 0) {
				return;
			}
			else if (static_cast<unsigned char> (buf[1]) < 128 ||
				static_cast<unsigned char> (buf[1]) > 130) {
				return;
			}
			else if (responce_pmp.result_code != 0) {
				return;
			}
			else {
				responce_pmp.type = static_cast<unsigned char>(buf[1]) & 0x7f;

				if (static_cast<unsigned char> (buf[1]) == 128) {
					uint32_t ip = ntohl(*(reinterpret_cast<const uint32_t*>(buf + 8)));

					responce_pmp.public_address = boost::asio::ip::address_v4(ip);

					m_public_ip_address_ = responce_pmp.public_address;

					d_timer.cancel();
				}
				else {
					responce_pmp.private_port =
						ntohs(*(reinterpret_cast<const uint16_t*>(buf + 8)));

					responce_pmp.public_port =
						ntohs(*(reinterpret_cast<const uint16_t*>(buf + 10)));

					responce_pmp.lifetime =
						ntohl(*(reinterpret_cast<const uint32_t*>(buf + 12)));
				}
			}
		}
		else {
			return;
		}

	}
	void Natpmp::HandleAsyncConnect(const boost::system::error_code & ec) 
	{
		if (ec == boost::asio::error::operation_aborted) {

		}
		if (!ec)
			GetExternalIPAddress();
	}
#ifdef WIN32
	int getdefaultgateway(std::string *rout_addr)
	{
		MIB_IPFORWARDROW ip_forward;
		memset(&ip_forward, 0, sizeof(ip_forward));
		if (GetBestRoute(inet_addr("0.0.0.0"), 0, &ip_forward) != NO_ERROR)
			return -1;
		std::string first = std::to_string(ip_forward.dwForwardNextHop & 0xFF);
		std::string second = std::to_string(ip_forward.dwForwardNextHop >> 8 & 0xFF);
		std::string third = std::to_string(ip_forward.dwForwardNextHop >> 16 & 0xFF);
		std::string forth = std::to_string(ip_forward.dwForwardNextHop >> 24 & 0xFF);
		std::string result;
		result += first + "." + second + "." + third + "." + forth;
		*rout_addr = result;
		return 0;
	};
#endif
};