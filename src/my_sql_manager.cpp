#include "../include/my_sql_manager.h"
#include "Arduino.h"
#include "MySQL_Cursor.h"
#include "arduino_secrets.h"

MySQLManager::MySQLManager(WiFiClient& client) : m_con(reinterpret_cast<Client*>(&client)) {}

void
MySQLManager::connect()
{
    IPAddress host_IP;
    Serial.println("Starting DB connection...");
    if(WiFi.hostByName(MYSQL_HOST, host_IP))
    {
        unsigned long const start = millis();
        while(!m_con.connect(host_IP, 3306, MYSQL_USER, MYSQL_PASSWORD))
        {
            if(millis() - start > 10000)
            {
                Serial.println("MySQL connection timeout.");
                return;
            }
            delay(5000);
        }

        MySQL_Cursor* cur = new MySQL_Cursor(&m_con);
        char query[64];
        snprintf(query, sizeof(query), "USE %s;", MYSQL_DATABASE);
        cur->execute(query);
        delete cur;
    }
    else
    {
        Serial.println("Host by name failed!");
    }
}

bool
MySQLManager::ensure_connection()
{
    if(!m_con.connected())
    {
        connect();
        return m_con.connected();
    }
    Serial.println("Connection ensured!");
    return true;
}

void
MySQLManager::insert_data(int counter)
{
    if(!ensure_connection())
    {
        return;
    }

    char query[256];
    snprintf(
        query, sizeof(query),
        "INSERT INTO debug (counter) VALUES "
        "(%d);",
        counter);

    MySQL_Cursor* cur = new MySQL_Cursor(&m_con);
    if(cur->execute(query))
    {
        Serial.println("Data inserted successfully!");
    }
    else
    {
        Serial.println("Inserting error!");
    }
    delete cur;
}