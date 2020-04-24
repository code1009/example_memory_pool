/////////////////////////////////////////////////////////////////////////////
//===========================================================================
#include "stdafx.h"

#include <conio.h>

#include <memory>

#include <Windows.h>





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
#define PROVIDER_COUNT 1



/////////////////////////////////////////////////////////////////////////////
//===========================================================================
typedef struct _thread_t
{
	HANDLE handle;
	DWORD  id;
	DWORD  periodic_time;
} thread_t;



/////////////////////////////////////////////////////////////////////////////
//===========================================================================
DWORD WINAPI provider_thread_function (LPVOID param);
DWORD WINAPI consumer_thread_function (LPVOID param);



/////////////////////////////////////////////////////////////////////////////
//===========================================================================
class message;
class message_queue;
class message_pool;

class provider;
class consumer;
class application;



/////////////////////////////////////////////////////////////////////////////
//===========================================================================
typedef enum _message_type_t
{
	MESSAGE_TYPE_UNKNOWN = 0,
	MESSAGE_TYPE_CODE,
	MESSAGE_TYPE_PACKET,
}
message_type_t;





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
class message
{
public:
	int type;

public:
	int provider;
	int sequence;

public:
	message();
	virtual ~message();
};





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
class code_message : public message
{
public:
	int code;

public:
	code_message();
	virtual ~code_message();
};





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
class packet_message : public message
{
public:
	unsigned char packet_buffer[1024];

public:
	packet_message();
	virtual ~packet_message();
};





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
class message_queue
{
public:
	typedef message* element_t;

public:
	int              _max_count ;
	int              _count     ;
	int              _r_position;
	int              _w_position;
	HANDLE           _r_counter ; 
	HANDLE           _w_counter ;  
	CRITICAL_SECTION _mutex     ;
	
	element_t* _element_array;

public:
	int  create (int max_count);
	void destroy();

public:
	int write (element_t e);
	int read  (element_t& e);
	int count (void);
	int push  (message*  m, DWORD timeout=INFINITE);
	int pop   (message*& m, DWORD timeout=INFINITE);
};





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
template<class T>
class typed_message_pool
{
public:
	CRITICAL_SECTION _mutex;
	int _max_count;

public:
	int  create  (int max_count);
	void destroy (void);

public:
	message* allocate   (void);
	void     deallocate (message* m);
};

//===========================================================================
template<class T>
int typed_message_pool<T>::create(int max_count)
{
	_max_count = max_count;

	InitializeCriticalSection(&_mutex);


	return 0;
}

template<class T>
void typed_message_pool<T>::destroy(void)
{

	DeleteCriticalSection(&_mutex);
}

template<class T>
message* typed_message_pool<T>::allocate(void)
{
	return NULL;
}

template<class T>
void typed_message_pool<T>::deallocate(message* m)
{

}



/////////////////////////////////////////////////////////////////////////////
//===========================================================================
class message_pool
{
public:
	typed_message_pool<code_message>   _code  ;
	typed_message_pool<packet_message> _packet;

public:
	int  create  (void);
	void destroy (void);

public:
	message* allocate   (int type);
	void     deallocate (int type, message* m);
};



/////////////////////////////////////////////////////////////////////////////
//===========================================================================
class provider
{
public:
	application* _application;
	int _id;

public:
	int _sequence;

public:
	provider();

public:
	void run (void);

public:
	void send_message (void);
	message* build_code_message (void);
	message* build_packet_message (void);
};





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
class consumer
{
public:
	application* _application;

public:
	consumer();

public:
	void run (void);

public:
	void recv_message (void);
	void process_message (message* m);
	void process_code_message (code_message* m);
	void process_packet_message (packet_message* m);
};





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
class application
{
public:
	HANDLE   _exit_event;

	thread_t _provider_thread[PROVIDER_COUNT];
	thread_t _consumer_thread;

	provider _provider[PROVIDER_COUNT];
	consumer _consumer;

	message_queue _message_queue;
	message_pool  _message_pool;

public:
	application();

public:
	int  create  (void);
	void destroy (void);

public:
	void run (void);

public:
	void wait_for_thread (void);
};





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
int _tmain(int argc, _TCHAR* argv[])
{
	application app;


	if (0==app.create())
	{
		app.run();
	}
	app.destroy();

	return 0;
}





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
application::application()
{
	int i;


	_exit_event = NULL;

	for (i=0; i<PROVIDER_COUNT; i++)
	{
		_provider_thread[i].handle = NULL;
	}

	_consumer_thread.handle = NULL;
}

//===========================================================================
int application::create (void)
{
	int i;


	//-----------------------------------------------------------------------
	if (0!=_message_pool.create())
	{
		goto error;
	}
	

	//-----------------------------------------------------------------------
	if (0!=_message_queue.create(3))
	{
		goto error;
	}


	//-----------------------------------------------------------------------
	_exit_event = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (NULL==_exit_event)
	{
		goto error;
	}


	//-----------------------------------------------------------------------
	DWORD time_base;


	time_base = 1000u;


	for (i=0; i<PROVIDER_COUNT; i++)
	{
		_provider[i]._application = this;
		_provider[i]._id = i;

		_provider_thread[i].periodic_time = 800 + 10*i;
		_provider_thread[i].handle = CreateThread(NULL, 0u, provider_thread_function, &_provider[i], CREATE_SUSPENDED, &_provider_thread[i].id);
		if (NULL==_provider_thread[i].handle)
		{
			goto error;
		}
	}


	//-----------------------------------------------------------------------
	_consumer._application = this;

	_consumer_thread.periodic_time = time_base;
	_consumer_thread.handle = CreateThread(NULL, 0u, consumer_thread_function, &_consumer, CREATE_SUSPENDED, &_consumer_thread.id);
	if (NULL==_consumer_thread.handle)
	{
		goto error;
	}


	//-----------------------------------------------------------------------
	ResumeThread(_consumer_thread.handle);

	for (i=0; i<PROVIDER_COUNT; i++)
	{
		ResumeThread(_provider_thread[i].handle);
	}

	return 0;


	//-----------------------------------------------------------------------
error:
	return -1;
}

void application::destroy (void)
{
	int i;

	
	if (NULL!=_exit_event)
	{
		CloseHandle(_exit_event);
		_exit_event = NULL;
	}


	if (NULL!=_consumer_thread.handle)
	{
		CloseHandle(_consumer_thread.handle);
		_consumer_thread.handle = NULL;
	}


	for (i=0; i<PROVIDER_COUNT; i++)
	{
		if (NULL!=_provider_thread[i].handle)
		{
			CloseHandle(_provider_thread[i].handle);
			_provider_thread[i].handle = NULL;
		}
	}

	_message_queue.destroy();

	_message_pool.destroy();
}

void application::run (void)
{
	int loop;
	
	int ch;


	loop=1;
	while (loop)
	{
		ch = _getch();
		switch (ch)
		{
		case 'x':
			loop = 0;
			break;

		default:
			break;
		}
	}

	SetEvent(_exit_event);

	wait_for_thread ();
}

void application::wait_for_thread (void)
{
	int i;

	DWORD  handle_count;
	HANDLE handle_array[1+PROVIDER_COUNT];

	DWORD object;


	handle_count = 1+PROVIDER_COUNT;

	handle_array[0]=_consumer_thread.handle;
	for (i=0; i<PROVIDER_COUNT; i++)
	{
		handle_array[1+i]=_provider_thread[i].handle;
	}

	object = WaitForMultipleObjects(handle_count, handle_array, TRUE, INFINITE);
	switch (object)
	{
	case WAIT_OBJECT_0:
		printf ("application::wait_for_thread(): WaitForMultipleObjects()==WAIT_OBJECT_0\n");
		break;

	case WAIT_TIMEOUT:
		printf ("application::wait_for_thread(): WaitForMultipleObjects()==WAIT_TIMEOUT\n");
		break;

	case WAIT_ABANDONED_0:
		printf ("application::wait_for_thread(): WaitForMultipleObjects()==WAIT_ABANDONED_0\n");
		break;

	case WAIT_FAILED:
		printf ("application::wait_for_thread(): WaitForMultipleObjects()==WAIT_FAILED\n");
		break;

	default:
		printf ("application::wait_for_thread(): WaitForMultipleObjects()==0x%08x\n", object);
		break;
	}
}





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
DWORD WINAPI provider_thread_function (LPVOID param)
{
	provider* o;


	o = reinterpret_cast<provider*>(param);
	o->run();

	return 0;
}





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
DWORD WINAPI consumer_thread_function (LPVOID param)
{
	consumer* o;


	o = reinterpret_cast<consumer*>(param);
	o->run();


	return 0;
}





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
int message_queue::create(int max_count)
{
	//-----------------------------------------------------------------------
	_max_count = max_count;

	_count      = 0;
	_r_position = 0;
	_w_position = 0;


	//-----------------------------------------------------------------------
	InitializeCriticalSection(&_mutex);


	//-----------------------------------------------------------------------
	_element_array = new (std::nothrow) element_t[_max_count];
	if (NULL==_element_array)
	{
		goto error;
	}


	//-----------------------------------------------------------------------
	_w_counter = CreateSemaphore(NULL, _max_count, _max_count, NULL);
	if (NULL==_w_counter)
	{
		goto error;
	}

	_r_counter = CreateSemaphore(NULL, 0,          _max_count, NULL);
	if (NULL==_r_counter)
	{
		goto error;
	}


	//-----------------------------------------------------------------------
	return 0;


	//-----------------------------------------------------------------------
error:
	return -1;
}

void message_queue::destroy(void)
{
	//-----------------------------------------------------------------------
	if (NULL!=_r_counter)
	{
		CloseHandle (_r_counter);    
	}
	_r_counter = NULL;


	if (NULL!=_w_counter)
	{
		CloseHandle (_w_counter);
	}
	_w_counter = NULL;


	//-----------------------------------------------------------------------
	if (NULL!=_element_array)
	{
		delete[] _element_array;
	}
	_element_array = NULL;


	//-----------------------------------------------------------------------
	DeleteCriticalSection(&_mutex);


	//-----------------------------------------------------------------------
	_max_count  = 0;
	_count      = 0;
	_r_position = 0;
	_w_position = 0;
}

int message_queue::write (element_t e)
{
	int rc;

	
	rc = -1;


	EnterCriticalSection(&_mutex);

	if ( _count < _max_count )
	{
		_element_array[_w_position] = e;

		_w_position = (_w_position + 1) % _max_count;
			
		_count++;


		rc = _count;
	}

	LeaveCriticalSection(&_mutex);  


	return rc;
}

int message_queue::read (element_t& e)
{
	int rc;

	
	rc = -1;


	EnterCriticalSection(&_mutex);

	if ( 0 < _count )
	{
		e = _element_array[_r_position];

		_r_position = (_r_position + 1) % _max_count;
			
		_count--;

		rc = _count;
	}

	LeaveCriticalSection(&_mutex);  


	return rc;
}

int message_queue::count (void)
{
	int result;


	EnterCriticalSection(&_mutex);
	
	result = _count;

	LeaveCriticalSection(&_mutex);   

	return result;
}

int message_queue::push(message* m, DWORD timeout)
{
	int rc;

	DWORD object;


	rc     = -1;
	object = WaitForSingleObject(_w_counter, timeout);

	switch (object)
	{
	case WAIT_OBJECT_0:
		if (0<=write(m))
		{
			rc = 1;
		}
		ReleaseSemaphore(_r_counter, 1, NULL);
		break;

	case WAIT_TIMEOUT :
		rc = 0;
		break;

	default:
		break;
	}


	return rc;
}

int message_queue::pop(message*& m, DWORD timeout)
{
	int rc;

	DWORD object;


	rc     = -1;
	object = WaitForSingleObject(_r_counter, timeout);

	switch (object)
	{
	case WAIT_OBJECT_0:
		if (0<=read(m))
		{
			rc = 1;
		}
		ReleaseSemaphore(_w_counter, 1, NULL);
		break;

	case WAIT_TIMEOUT :
		rc = 0;
		break;

	default:
		break;
	}


	return rc;
}





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
provider::provider():
	_application(NULL),
	_id(-1),
	_sequence(0)
{
}

void provider::run (void)
{
	int loop;

	DWORD timeout;
	DWORD object;


	timeout = _application->_provider_thread[_id].periodic_time;

	loop = 1;
	while (loop)
	{
		object = WaitForSingleObject(_application->_exit_event, timeout);
		switch (object)
		{
		case WAIT_OBJECT_0:
			loop = 0;
			break;

		case WAIT_TIMEOUT:
			send_message();
			break;

		case WAIT_ABANDONED_0:
			printf ("provider::run(): WaitForSingleObject()==WAIT_ABANDONED_0\n");
			loop = 0;
			break;

		case WAIT_FAILED:
			printf ("provider::run(): WaitForSingleObject()==WAIT_FAILED\n");
			loop = 0;
			break;

		default:
			printf ("provider::run(): WaitForSingleObject()==0x%08x\n", object);
			loop = 0;
			break;
		}
	}
}

void provider::send_message (void)
{
	int rc;

	message* m;


	if (0==(_sequence%2))
	{
		m = build_code_message();
	}
	else
	{
		m = build_packet_message();
	}

	if (m)
	{
		rc = _application->_message_queue.push(m);
		if (rc<0)
		{
			printf ("provider::send_message(): [error] queue push\n");
		}
	}
	else
	{
		printf ("provider::send_message(): [error] allocate\n");
	}

	_sequence++;
}

message* provider::build_code_message (void)
{
	code_message* m;

	
	m = static_cast<code_message*>( _application->_message_pool.allocate(MESSAGE_TYPE_CODE) );
	if (m)
	{
		m->provider = _id;
		m->sequence = _sequence;

		m->code = _sequence;
	}

	return m;
}

message* provider::build_packet_message (void)
{
	packet_message* m;

	
	m = static_cast<packet_message*>( _application->_message_pool.allocate(MESSAGE_TYPE_PACKET) );
	if (m)
	{
		m->provider = _id;
		m->sequence = _sequence;

		memcpy(m->packet_buffer, &_sequence, sizeof(int));
	}

	return m;
}





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
consumer::consumer():
	_application(NULL)
{
}

void consumer::run (void)
{
	int loop;

	DWORD  handle_count;
	HANDLE handle_array[2];

	DWORD timeout;

	DWORD object;


	handle_count = 2;

	handle_array[0] = _application->_exit_event;
	handle_array[1] = _application->_message_queue._r_counter;

	timeout = _application->_consumer_thread.periodic_time;

	loop = 1;
	while (loop)
	{
		object = WaitForMultipleObjects(handle_count, handle_array, FALSE, timeout);
		switch (object)
		{
		case WAIT_OBJECT_0:
			loop = 0;
			break;

		case WAIT_OBJECT_0+1:
			recv_message();
			break;

		case WAIT_TIMEOUT:
			printf ("consumer::run(): WaitForMultipleObjects()==WAIT_TIMEOUT\n");
			break;

		case WAIT_ABANDONED_0:
			printf ("consumer::run(): WaitForMultipleObjects()==WAIT_ABANDONED_0\n");
			loop = 0;
			break;

		case WAIT_FAILED:
			printf ("consumer::run(): WaitForMultipleObjects()==WAIT_FAILED\n");
			loop = 0;
			break;

		default:
			printf ("consumer::run(): WaitForMultipleObjects()==0x%08x\n", object);
			loop = 0;
			break;
		}
	}
}

void consumer::recv_message (void)
{
	int rc;

	message* m;


	m = (message*)0;

	rc = _application->_message_queue.read(m);
	ReleaseSemaphore(_application->_message_queue._w_counter, 1, NULL);

	printf ("consumer::recv_message(): queue read(%d) = %d \n", rc, (int)m);

	if (0<=rc)
	{
		process_message(m);
	}
}

void consumer::process_message (message* m)
{
	switch (m->type)
	{
	case MESSAGE_TYPE_CODE:   process_code_message  (static_cast<code_message*  >(m)); _application->_message_pool.deallocate(m->type, m); break;
	case MESSAGE_TYPE_PACKET: process_packet_message(static_cast<packet_message*>(m)); _application->_message_pool.deallocate(m->type, m); break;

	default:
		break;
	}
}

void consumer::process_code_message (code_message* m)
{
	int v;


	v = m->code;

	printf ("[consumer] (%d,%d) code %d \n", m->provider, m->sequence, v);
}

void consumer::process_packet_message (packet_message* m)
{
	int v;


	memcpy(&v, m->packet_buffer, sizeof(int));

	printf ("[consumer] (%d,%d) packet %d \n", m->provider, m->sequence, v);
}





/////////////////////////////////////////////////////////////////////////////
//===========================================================================
int message_pool::create (void)
{
	int rc;


	rc = 0;


	if (0>_code  .create(3)){rc = -1;}
	if (0>_packet.create(3)){rc = -1;}
	
	return rc;
}

void message_pool::destroy (void)
{
	_packet.destroy();
	_code  .destroy();
}

message* message_pool::allocate (int type)
{
	message* m;


	m = NULL;
	switch (type)
	{
	case MESSAGE_TYPE_CODE   : m = _code  .allocate(); break;
	case MESSAGE_TYPE_PACKET : m = _packet.allocate(); break;

	default:
		printf ("message_pool::allocate(): [error] type\n");
		break;
	}

	return m;
}

void message_pool::deallocate (int type, message* m)
{
	switch (type)
	{
	case MESSAGE_TYPE_CODE   : _code  .deallocate(m); break;
	case MESSAGE_TYPE_PACKET : _packet.deallocate(m); break;

	default:
		printf ("message_pool::deallocate(): [error] type\n");
		break;
	}
}




