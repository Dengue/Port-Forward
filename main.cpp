#include "upnp.cpp"
#include "natpmp.cpp"
#include <conio.h>
#define MIN_PORT_NUMBER 2000

void ConsoleInterface()
{
	std::cout << "a - add new port mapping" << std::endl;
	std::cout << "d - delete existing port mapping" << std::endl;
	std::cout << "s - show all port mappings" << std::endl;
	std::cout << "e - exit" << std::endl;
}
void UserInput(char choose,std::string * external_port, std::string * internal_ip,
	std::string *internal_port, std::string * protocol, std::string * port_forwading_description)
{
	bool rez;
	switch (choose) {
	case 'a':{
				 
				 do{
					 system("cls");
					 std::cout << "Enter protocol (TCP or UDP)" << std::endl;
					 std::cin >> *protocol;
					 if (!protocol->compare("TCP"))
						 break;
					 if (!protocol->compare("UDP"))
						 break;
					 std::cout << "Incorrect input. Try again";
					 _getch();
				 } while (1);
				 do{
					 system("cls");
					 std::cout << "Enter internal ip address (format: 192.168.xxx.xxx)" << std::endl;
					 std::cin >> *internal_ip;
					 boost::regex xRegEx("192.168.\\d{1,3}.\\d{1,3}");
					 boost::smatch xResults;
					 rez = boost::regex_match(*internal_ip, xResults, xRegEx);
					 if (rez == false)
					 {
						 std::cout << "Incorrect input. Try again";
						 _getch();
					 }
				 } while (rez==false);
				 int i_port;
				 do {
					 system("cls");
					 std::cout << "Enter internal port (should be > 2000 and < 65565)" << std::endl;
					 std::cin >> i_port;
					 if (i_port < MIN_PORT_NUMBER || i_port > 65565)
					 {
						 std::cout << "Incorrect input. Try again";
						 _getch();
					 }
				 } while (i_port < MIN_PORT_NUMBER || i_port > 65565);
				 *internal_port = std::to_string(i_port);
				 do{
					 system("cls");
					 std::cout << "Enter external port (should be equal to internal port)" << std::endl;
					 std::cin >> *external_port;
					 if (external_port->compare(*internal_port))
					 {
						 std::cout << "Incorrect input. Try again";
						 _getch();
					 }
				 } while (external_port->compare(*internal_port));
				 system("cls");
				 std::cout << "Enter description name" << std::endl;
				 std::cin >> *port_forwading_description;
				 break;
	}
	case 'd':{
				 do{
					 system("cls");
					 std::cout << "Enter protocol (TCP or UDP)" << std::endl;
					 std::cin >> *protocol;
					 if (!protocol->compare("TCP"))
						 break;
					 if (!protocol->compare("UDP"))
						 break;
					 std::cout << "Incorrect input. Try again";
					 _getch();
				 } while (1);
				 int i_port;
				 do {
					 system("cls");
					 std::cout << "Enter internal port (should be > 2000 and < 65565)" << std::endl;
					 std::cin >> i_port;
					 if (i_port < MIN_PORT_NUMBER || i_port > 65565)
					 {
						 std::cout << "Incorrect input. Try again";
						 _getch();
					 }
				 } while (i_port < MIN_PORT_NUMBER || i_port > 65565);
				 *internal_port = std::to_string(i_port);
				 do{
					 system("cls");
					 std::cout << "Enter external port (should be equal to internal port)" << std::endl;
					 std::cin >> *external_port;
					 if (external_port->compare(*internal_port))
					 {
						 std::cout << "Incorrect input. Try again";
						 _getch();
					 }
				 } while (external_port->compare(*internal_port));
				 break;
	}
	}
}
int main()
{
	Natpmp natpmp;
	natpmp.NewPortMapping(1, 14000, 14000);
	natpmp.DeletePortMapping(1, 14000, 14000);
	Upnp upnp;
	std::string search_result, location, device_description, wan_ip_service_descrption, external_ip;
	if (upnp.SearchForRouter(&search_result) != OK)
		return FAIL;
	if (upnp.FindRouterLocation(search_result, &location) != OK)
		return FAIL;
	if (upnp.GetWanIpServiceDescrption(location, &wan_ip_service_descrption) != OK)
		return FAIL;
	if (upnp.GetExternalIpAddress(location, &external_ip) != OK)
		return FAIL;
	for (int i = 0; upnp.GetGenericPortMappingEntry(location, i) == OK; i++);
	char choose;
	do{
		ConsoleInterface();
		switch (choose = getchar()) {
		case 'a':{
					 int protocol_pmp,internal_por_pmp,external_port_pmp;
					 std::string external_ip, external_port, internal_ip, internal_port, protocol, port_forwading_description;
					 system("cls");
					 UserInput(choose, &external_port, &internal_ip, &internal_port, &protocol, &port_forwading_description);
					 if (!protocol.compare("TCP"))
						 protocol_pmp = 1;
					 else
						 protocol_pmp = 2;
					 std::istringstream ist1(internal_port);
					 ist1 >> internal_por_pmp;
					 std::istringstream ist2(external_port);
					 ist2 >> external_port_pmp;
					 natpmp.NewPortMapping(protocol_pmp, internal_por_pmp, external_port_pmp);
					 if (upnp.NewPortMapping(location, "", external_port, internal_ip, internal_port, protocol, port_forwading_description) != OK)
						 return FAIL;
					 std::cout << "New port mapping added";
					 _getch();
					 break;
		}
		case 'd':
		{
					int protocol_pmp, internal_por_pmp, external_port_pmp;
					std::string external_ip, external_port, internal_ip, internal_port, protocol, port_forwading_description;
					system("cls");
					UserInput(choose, &external_port, &internal_ip, &internal_port, &protocol, &port_forwading_description);
					if (!protocol.compare("TCP"))
						protocol_pmp = 1;
					else
						protocol_pmp = 2;
					std::istringstream ist1(internal_port);
					ist1 >> internal_por_pmp;
					std::istringstream ist2(external_port);
					ist2 >> external_port_pmp;
					natpmp.DeletePortMapping(protocol_pmp, internal_por_pmp, external_port_pmp);
					if (upnp.DeletePortMapping(location, "", external_port, protocol) != OK)
						return FAIL;
					std::cout << "port mapping deleted";
					_getch();
					break;
		}
		case 's':system("cls"); upnp.SeeAllPortMappings(); _getch();
		case 'e':break;
		default:system("cls"); break;
		}
	} while (choose != 'e');
	return OK;
}