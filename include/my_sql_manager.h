#pragma once

#include <MySQL_Connection.h>
#include <MySQL_Cursor.h>
#include <WiFi.h>

class MySQLManager
{
public:
    MySQLManager(WiFiClient& client);

    void
    connect();

    void
    insert_data(int counter);

    bool
    ensure_connection();

private:
    MySQL_Connection m_con;
};