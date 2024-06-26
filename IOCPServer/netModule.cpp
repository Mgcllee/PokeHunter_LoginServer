﻿#pragma once

#include "DBModule.h"

std::array<PLAYER, MAX_USER> clients;
std::array<PARTY, MAX_PARTY> parties;


OVER_EXP::OVER_EXP() :
	_send_buf("")
{
	ZeroMemory(&_over, sizeof(_over));
	_wsabuf.len = BUF_SIZE;
	_wsabuf.buf = _send_buf;
	c_type = RECV;
}
OVER_EXP::OVER_EXP(char* packet) :
	_send_buf("")
{
	_wsabuf.len = packet[0];
	_wsabuf.buf = _send_buf;
	ZeroMemory(&_over, sizeof(_over));
	c_type = SEND;
	memcpy(_send_buf, packet, packet[0]);
}
OVER_EXP::~OVER_EXP()
{
	free(&_over);
	free(&_wsabuf);
	free(&c_type);
}

SESSION::SESSION() :
	_prev_size(0)
	/*, _recv_over(NULL)
	, _socket(NULL)*/
{
	_socket = NULL;
}
SESSION::~SESSION()
{
	closesocket(_socket);
}
bool SESSION::recycle_session()
{
	return false;
}
SESSION& SESSION::operator=(const SESSION& ref)
{
	this->_recv_over = ref._recv_over;
	this->_socket = ref._socket;
	this->_prev_size = ref._prev_size;
	return *this;
}
void SESSION::set_socket(SOCKET new_socket)
{
	_socket = new_socket;

}
int SESSION::get_prev_size()
{
	return _prev_size;
}
void SESSION::set_prev_size(int in)
{
	_prev_size = in;
}
void SESSION::do_recv()
{
	DWORD recv_flag = 0;
	memset(&_recv_over._over, 0, sizeof(_recv_over._over));
	_recv_over._wsabuf.len = BUF_SIZE - _prev_size;
	_recv_over._wsabuf.buf = _recv_over._send_buf + _prev_size;
	_recv_over.c_type = RECV;
	WSARecv(_socket, &_recv_over._wsabuf, 1, 0, &recv_flag, &_recv_over._over, 0);
}
void SESSION::do_send(void* packet)
{
	if (packet == nullptr) return;
	if (_socket == NULL) return;

	OVER_EXP* send_over = new OVER_EXP{ reinterpret_cast<char*>(packet) };
	WSASend(_socket, &send_over->_wsabuf, 1, 0, 0, &send_over->_over, 0);
}
void SESSION::disconnect()
{
	_recv_over.~OVER_EXP();
	closesocket(_socket);
}

PLAYER::PLAYER() :
	_uid(-1)
	, _name("None")
	, _pet_num("None")
	, _player_state(ST_HOME)
{
	
}
PLAYER::PLAYER(const PLAYER& rhs) :
	_uid(-1)
	, _name("None")
	, _pet_num("None")
	, _player_state(ST_HOME)
{

}
PLAYER::~PLAYER()
{

}
void PLAYER::recycle_player()
{

}
SESSION* PLAYER::get_session()
{
	return &_session;
}
std::string* PLAYER::get_token()
{
	return &IdToken;
}
PLAYER& PLAYER::operator=(const PLAYER& ref)
{
	if (this != &ref)
	{
		std::lock(this->ll, ref.ll);
		std::lock_guard<std::mutex> this_lock(this->ll, std::adopt_lock);
		std::lock_guard<std::mutex> ref_lock(ref.ll, std::adopt_lock);

		strncpy_s(this->_name, ref._name, strlen(ref._name));
		this->_uid = ref._uid;
		strncpy_s(this->_pet_num, ref._pet_num, strlen(ref._pet_num));
		this->_player_skin = ref._player_skin;
		this->_player_state = ref._player_state;
		this->_session = ref._session;
		this->inventory_data = ref.inventory_data;
		this->storage_data = ref.storage_data;

	}

	return *this;
}
bool PLAYER::operator== (const PLAYER & rhs) const
{
	return _uid == rhs._uid;
}
char* PLAYER::get_name()
{
	return _name;
}
void PLAYER::set_name(const char* in)
{
	strncpy_s(_name, sizeof(in), in, sizeof(in));
}
void PLAYER::set_item(const char* in_item_name, short index, char cnt)
{
	
}
short PLAYER::get_item()
{
	return 0;
}
short PLAYER::get_storage_item()
{
	return 0;
}
void PLAYER::send_fail(const char* fail_reason)
{
	SC_FAIL_PACK fail_pack;
	fail_pack.size = sizeof(SC_FAIL_PACK);
	fail_pack.type = SC_FAIL;
	_session.do_send(&fail_pack);
	printf("[Fail Log][%d] : %s\n", _uid, fail_reason);
}
void PLAYER::send_self_info(const char* success_message)
{
	SC_LOGIN_INFO_PACK info_pack = SC_LOGIN_INFO_PACK();
	info_pack.size = (char)sizeof(info_pack);
	info_pack.type = SC_LOGIN_INFO;

	strcpy_s(info_pack.name, _name);
	info_pack._player_skin = 1;
	info_pack._pet_num = 1;
	
	_session.do_send(&info_pack);

	printf("[Success Log][%d] : %s\n", _uid, success_message);
}

PARTY::PARTY() :
	_mem_count(0)
	, _inStage(false)
	, _name("Empty")
{

}
PARTY::~PARTY()
{
	
}
bool PARTY::new_member(PLAYER& new_mem)
{
	if (_mem_count >= PARTY_MAX_NUM) 
		return false;
	
	ll.lock();
	member.push_back(new_mem);
	++_mem_count;
	ll.unlock();

	return true;
}
bool PARTY::leave_member(char* mem_name) {
	if (_mem_count <= 0) 
		return false;

	ll.lock();
	for (PLAYER& mem : member) 
	{
		if (0 == strcmp(mem.get_name(), mem_name)) 
		{
			member.remove(mem);
			if (0 == --_mem_count)
				_inStage = false;

			ll.unlock();
			return true;
		}
	}

	ll.unlock();
	return false;
}
bool PARTY::get_party_in_stage()
{
	return _inStage;
}
short PARTY::get_mem_count()
{
	return _mem_count;
}

void process_packet(int c_uid, char* packet)
{
	switch (packet[1]) {
	case CS_LOGIN:
	{
		CS_LOGIN_PACK* token_pack = reinterpret_cast<CS_LOGIN_PACK*>(packet);

		if (0 == strncmp(token_pack->Token, "dummy_client_", strlen("dummy_client_"))) {
			std::string nameBuffer = token_pack->Token;
			nameBuffer = nameBuffer.erase(0, strlen("dummy_client_"));
			clients[c_uid].set_name(nameBuffer.c_str());
			printf("dummy client name : %s\n", clients[c_uid].get_name());
			clients[c_uid].send_self_info(clients[c_uid].get_name());
			break;



			/*if (Login_UDB(c_uid, nameBuffer)) {
				clients[c_uid].send_self_info(
					std::string("Login player name is ").append(clients[c_uid].get_name()).c_str()
				);
			}
			else {
				if (SetNew_UDB(c_uid, nameBuffer)) {
					clients[c_uid].send_self_info(
						std::string("New login player name is ").append(clients[c_uid].get_name()).c_str()
					);
				}
				else {
					clients[c_uid].send_fail("Failed to initialize player");
				}
			}*/
		}
		else if (0 == strncmp(token_pack->Token, "theEnd", strlen("theEnd"))) {
			
			// AWS Cognito API를 사용하는 함수.
			
			// std::string nameBuffer = GetPlayerName(*clients[c_uid].get_token());
			std::string nameBuffer;

			if ("TokenError" == nameBuffer || "Token Error" == nameBuffer || "Empty" == nameBuffer) {
				clients[c_uid].send_fail("Token error");
			}
			/*else if (Login_UDB(c_uid, nameBuffer)) {
				clients[c_uid].send_self_info(
					std::string("Login player name is ").append(clients[c_uid].get_name()).c_str()
				);
			}
			else {
				if (SetNew_UDB(c_uid, nameBuffer)) {
					clients[c_uid].send_self_info(
						std::string("New login player name is ").append(clients[c_uid].get_name()).c_str()
					);
				}
				else {
					clients[c_uid].send_fail("Failed to initialize player");
				}
			}*/
		}
		else {
			std::string tokenBuffer;
			tokenBuffer.assign(token_pack->Token, (size_t)token_pack->Token_size);
			clients[c_uid].get_token()->append(tokenBuffer);
		}
 	}
	break;
	
	case CS_CHAT:
	{
		CS_CHAT_PACK* recv_packet = reinterpret_cast<CS_CHAT_PACK*>(packet);
		std::string chat_log = std::string("[채팅][").append(clients[c_uid].get_name()).append("]: ").append(recv_packet->content);
		
		printf("%s\n", chat_log.c_str());

		SC_CHAT_PACK ctp;
		ctp.size = sizeof(SC_CHAT_PACK);
		ctp.type = SC_CHAT;
		strcpy_s(ctp.content, chat_log.c_str());
		
		for (PLAYER& p : clients)
		{
			p.get_session()->do_send(&ctp);
		}
	}
	break;

	case CS_QUEST_INVENTORY:
	{
		// Get_ALL_ItemDB(c_uid);
		 
		SC_ITEM_INFO_PACK item_pack;
		item_pack.size = sizeof(SC_ITEM_INFO_PACK);
		item_pack.type = SC_ITEM_INFO;

		/*

		for (int category = 0; category < MAX_ITEM_CATEGORY; ++category) {
			for (int item_num = 0; item_num < MAX_ITEM_COUNT; ++item_num) {

				std::string item_name = Get_ItemName(category, item_num);
				int cnt = clients[c_uid].get_item_arrayName(category)[item_num];

				if (false == item_name.compare("None")) continue;
				if (0 == cnt) continue;

				printf("[%s] : %d\n", item_name.c_str(), cnt);
				{
					std::lock_guard<std::mutex> ll{ clients[c_uid]._lock };
					// clients[c_uid].itemData.insert({ item_name, cnt });
				}

				strncpy_s(item_pack._name, CHAR_SIZE, item_name.c_str(), strlen(item_name.c_str()));
				item_pack._cnt = cnt;
				// clients[c_uid].do_send(&item_pack);
			}
		}
		
		*/

		strncpy_s(item_pack._name, CHAR_SIZE, "theEnd", sizeof("theEnd"));
		// clients[c_uid].do_send(&item_pack);
		std::cout << "End\n";
	}
	break;
	case CS_SAVE_INVENTORY:
	{
		CS_SAVE_INVENTORY_PACK* save_pack = reinterpret_cast<CS_SAVE_INVENTORY_PACK*>(packet);
		short cnt = save_pack->_cnt;
		
		break;

		/*
		
		std::map<std::string, short>::iterator info = clients[c_uid].itemData.find(save_pack->_name);
		if (clients[c_uid].itemData.end() != info) {
			std::lock_guard<std::mutex> ll{ clients[c_uid]._lock };
			info->second = cnt;
		}
		else if (0 == strcmp(save_pack->_name, "theEnd")) {
			for (int category = 0; category < MAX_ITEM_CATEGORY; ++category) {
				for (int item_num = 0; item_num < MAX_ITEM_COUNT; ++item_num) {

					std::map<std::string, short>::iterator it = clients[c_uid].itemData.find(Get_ItemName(category, item_num));
					if (clients[c_uid].itemData.end() != it) {
						clients[c_uid].get_item_arrayName(category)[item_num] = it->second;
						std::cout << "<" << it->first << "> - " << it->second << std::endl;
					}
					else {
						clients[c_uid].get_item_arrayName(category)[item_num] = 0;
						std::cout << "<" << it->first << "> - " << 0 << std::endl;
					}
				}
			}
		}
		
		*/
	}
	break;
	case CS_QUEST_STORAGE:
	{
		// Get_ALL_StorageDB(c_uid);

		SC_ITEM_INFO_PACK item_pack;
		item_pack.size = sizeof(SC_ITEM_INFO_PACK);
		item_pack.type = SC_ITEM_INFO;

		/*
		
		for (int category = 0; category < MAX_ITEM_CATEGORY; ++category) {
			for (int item_num = 0; item_num < MAX_ITEM_COUNT; ++item_num) {

				std::string item_name = Get_ItemName(category, item_num);
				int cnt = clients[c_uid].get_storage_item_arrayName(category)[item_num];
				if (false == item_name.compare("None"))continue;
				else if (0 == cnt)							continue;

				printf("[%s] : %d\n", item_name.c_str(), cnt);

				strncpy_s(item_pack._name, CHAR_SIZE, item_name.c_str(), strlen(item_name.c_str()));
				item_pack._cnt = cnt;
				clients[c_uid].do_send(&item_pack);
			}
		}

		strncpy_s(item_pack._name, CHAR_SIZE, "theEnd", sizeof("theEnd"));
		clients[c_uid].do_send(&item_pack);
		
		*/
	}
	break;
	case CS_PARTY_SEARCHING:
	{
		SC_PARTIES_INFO_PACK party_list;
		party_list.size = sizeof(SC_PARTIES_INFO_PACK);
		party_list.type = SC_PARTY_LIST_INFO;

		for (int i = 0; i < MAX_PARTY; ++i) {
			party_list._staff_count[i] = static_cast<char>(parties[i].get_mem_count());
			
			if (parties[i].get_party_in_stage()) 
				party_list.Inaccessible[i] = 1;
			else 
				party_list.Inaccessible[i] = 0;
		}

		clients[c_uid].get_session()->do_send(&party_list);
	}
	break;
	case CS_PARTY_ENTER:
	{
		CS_PARTY_ENTER_PACK* party_info = reinterpret_cast<CS_PARTY_ENTER_PACK*>(packet);
		int party_number = static_cast<int>(party_info->party_num);
		if (0 > party_number || party_number > 8) break;

		/*

		int cur_party_member_count = parties[party_number]._mem_count;
		if (0 <= cur_party_member_count && cur_party_member_count <= 3) {

			strncpy_s(parties[party_number].member[cur_party_member_count]._name, CHAR_SIZE, clients[c_uid]._name, CHAR_SIZE);
			parties[party_number].member[cur_party_member_count]._uid = c_uid;

			strncpy_s(parties[party_number].member[cur_party_member_count]._pet_num, CHAR_SIZE, clients[c_uid]._pet_num, CHAR_SIZE);
			{
				std::lock_guard<std::mutex> ll{ clients[c_uid]._lock };
				parties[party_number].member[cur_party_member_count]._player_state = ST_NOTREADY;
				clients[c_uid]._player_state = ST_NOTREADY;
			}
			
			parties[party_number]._mem_count += 1;

			SC_PARTY_ENTER_OK_PACK ok_pack;
			ok_pack.size = sizeof(SC_PARTY_ENTER_OK_PACK);
			ok_pack.type = SC_PARTY_ENTER_OK;
			clients[c_uid].do_send(&ok_pack);
			clients[c_uid]._party_num = party_number;

			printf("%s party connection Success\n", clients[c_uid].get_name());
		}
		else {
			printf("%s party connection failed\n", clients[c_uid].get_name());
		}

		*/
		
	}
	break;
	case CS_PARTY_INFO:
	{
		SC_PARTY_INFO_PACK in_party;
		in_party.size = sizeof(SC_PARTY_INFO_PACK);
		in_party.type = SC_PARTY_INFO;

		/*

		for (SESSION& cl : parties[clients[c_uid]._party_num].member) {
			if (0 == strcmp("None", cl._name)) continue;
			else  if (-1 == cl._uid)						continue;

			strncpy_s(in_party._mem, CHAR_SIZE, cl._name, CHAR_SIZE);
			strncpy_s(in_party._mem_pet, CHAR_SIZE, cl._pet_num, CHAR_SIZE);
			
			{
				std::lock_guard<std::mutex> ll{ cl._lock };
				if (ST_READY == cl._player_state) {
					in_party._mem_state = 2;
				}
				else in_party._mem_state = 0;
			}

			clients[c_uid].do_send(&in_party);
		}

		strncpy_s(in_party._mem, CHAR_SIZE, "theEnd", strlen("theEnd"));
		strncpy_s(in_party._my_name, CHAR_SIZE, clients[c_uid]._name, strlen(clients[c_uid]._name));
		clients[c_uid].do_send(&in_party);

		*/
	}
	break;
	case CS_PARTY_READY:
	{
		/*

		int cur_member = 0;
		int ready_member = 0;
		int party_num = clients[c_uid]._party_num;
		
		{
			std::lock_guard<std::mutex> ll{ clients[c_uid]._lock };
			if (clients[c_uid]._player_state != ST_READY) {
				clients[c_uid]._player_state = ST_READY;
			}
		}
		
		SC_PARTY_JOIN_RESULT_PACK result_pack;
		result_pack.size = sizeof(SC_PARTY_JOIN_RESULT_PACK);
		result_pack.type = SC_PARTY_JOIN_SUCCESS;

		int curMem = 0;
		for (SESSION& cl : parties[party_num].member) {
			if (0 != strcmp("None", cl._name)) {
				cur_member += 1;
			}

			if (c_uid == cl._uid) {
				cl._player_state = ST_READY;
				result_pack.memberState[curMem] = 2;
			}

			{
				if (ST_READY == cl._player_state) {
					ready_member += 1;
					result_pack.memberState[curMem] = 2;
				}
				else result_pack.memberState[curMem] = 0;
			}

			curMem += 1;
		}

		// if ((ready_member == curMem) && (false == parties[party_num]._inStage)) {
		if ((ready_member == cur_member)) {
			result_pack._result = 1;
			parties[party_num]._inStage = true;
			for (SESSION& cl : parties[party_num].member) {
				if (0 != strcmp("None", cl._name)) {
					printf("%s in Stage!\n", clients[cl._uid]._name);
					clients[cl._uid].do_send(&result_pack);
				}
			}
		}
		// else if(false == parties[party_num]._inStage) {
		else {
			result_pack._result = 0;
			
			for (SESSION& cl : parties[party_num].member) {
				if (0 != strcmp("None", cl._name)) {
					clients[cl._uid].do_send(&result_pack);
				}
			}
		}

		*/
	}
	break;
	case CS_PARTY_LEAVE:
	{
		/*
		
		int party_num = clients[c_uid]._party_num;

		if (0 <= party_num) {
			if (parties[party_num].leave_member(clients[c_uid]._name)) {
				clients[c_uid]._player_state = ST_HOME;
				clients[c_uid]._party_num = -1;

				SC_PARTY_LEAVE_SUCCESS_PACK leave_pack;
				leave_pack.size = sizeof(SC_PARTY_LEAVE_SUCCESS_PACK);
				leave_pack.type = SC_PARTY_LEAVE_SUCCESS;
				strncpy_s(leave_pack._mem, CHAR_SIZE, clients[c_uid]._name, CHAR_SIZE);
				clients[c_uid].do_send(&leave_pack);
				printf("[%s] left the party.\n", clients[c_uid].get_name());
			}
			else {
				SC_PARTY_LEAVE_FAIL_PACK fail_leave_pack;
				fail_leave_pack.size = sizeof(SC_PARTY_LEAVE_FAIL_PACK);
				fail_leave_pack.type = SC_PARTY_LEAVE_FAIL;
				clients[c_uid].do_send(&fail_leave_pack);
			}
		}
		else {
			printf("[%s] not [%d]party member.\n", clients[c_uid].get_name(), party_num);
			SC_PARTY_LEAVE_FAIL_PACK fail_leave_pack;
			fail_leave_pack.size = sizeof(SC_PARTY_LEAVE_FAIL_PACK);
			fail_leave_pack.type = SC_PARTY_LEAVE_FAIL;
			clients[c_uid].do_send(&fail_leave_pack);
		}

		*/
	}
	break;
	case CS_LOGOUT:
	{
		CS_LOGOUT_PACK* logout_client = reinterpret_cast<CS_LOGOUT_PACK*>(packet);

		SC_LOGOUT_RESULT_PACK out_client;
		out_client.size = sizeof(SC_LOGOUT_RESULT_PACK);
		out_client.type = SC_LOGOUT_RESULT;


		/*
		if (Set_ALL_ItemDB(c_uid) && Logout_UDB(c_uid)) {
			printf("[Success]->SaveData Logout %s\n", clients[c_uid].get_name());
		}
		else {
			printf("[Fail]->SaveData Logout %s\n", clients[c_uid].get_name());
		}

		{
			std::lock_guard<std::mutex> ll{ clients[c_uid]._lock };
			strcpy_s(out_client._result, "1");
			clients[c_uid].do_send(&out_client);
		}
		
		*/

		disconnect(c_uid);
	}
	break;
	}
}
void disconnect(int c_uid)
{
	clients[c_uid].get_session()->~SESSION();
}
void worker_thread(HANDLE h_iocp)
{
	while (true) {
		DWORD num_bytes;
		ULONG_PTR key;
		WSAOVERLAPPED* over = nullptr;
		BOOL ret = GetQueuedCompletionStatus(h_iocp, &num_bytes, &key, &over, INFINITE);
		OVER_EXP* ex_over = reinterpret_cast<OVER_EXP*>(over);

		// error 검출기
		if (FALSE == ret) {
			if (ex_over->c_type == ACCEPT) {
				std::cout << "Accept error\n";
			}
			else {
				std::cout << "\nGQCS Error on client[" << key << "]\n\n";
				// disconnect client
			}
		}
		if ((0 == num_bytes) && (ex_over->c_type == RECV)) continue;

		switch (ex_over->c_type) {
		case ACCEPT:
		{
			int new_c_uid = -1;

			if (-1 != new_c_uid) 
			{
				
				clients[new_c_uid]._lock();
				
				clients[new_c_uid].get_session()->set_socket(g_c_socket);
				clients[new_c_uid].set_uid(new_c_uid);
				clients[new_c_uid]._unlock();

				CreateIoCompletionPort(reinterpret_cast<HANDLE>(g_c_socket), h_iocp, new_c_uid, 0);

				clients[new_c_uid].get_session()->do_recv();
				g_c_socket = WSASocket(AF_INET, SOCK_STREAM, 0, NULL, 0, WSA_FLAG_OVERLAPPED);
			}
			else 
			{
				printf("connect fail\n");
				break;
			}

			ZeroMemory(&g_a_over._over, sizeof(g_a_over._over));
			int addr_size = sizeof(SOCKADDR_IN);
			AcceptEx(g_s_socket, g_c_socket, g_a_over._send_buf, 0, addr_size + 16, addr_size + 16, 0, &g_a_over._over);
		}
		break;
		case RECV:
		{
			int remain_data = num_bytes + clients[key].get_session()->get_prev_size();
			char* p = ex_over->_send_buf;

			while (remain_data > 0) {
				int packet_size = p[0];
				if (packet_size <= remain_data) {
					process_packet(static_cast<int>(key), p);
					p = p + packet_size;
					remain_data = remain_data - packet_size;
				}
				else break;
			}

			clients[key].get_session()->set_prev_size(remain_data);

			if (remain_data > 0) {
				memcpy(ex_over->_send_buf, p, remain_data);
			}
			clients[key].get_session()->do_recv();
		}
		break;
		case SEND:		
			delete ex_over;
			break;
		}
	}
}