#include "../include/data_manager.h"
#include "arduino_secrets.h"

void
DataManager::set_data_indicator(int mqtt_data)
{
    m_data_indicator = static_cast<char>(mqtt_data);
}

char
DataManager::get_data_indicator() const
{
    return m_data_indicator;
}

bool
DataManager::receive_data(int mqtt_data, size_t idx)
{
    if(idx < MAX_DATA_COUNT - 1)
    {
        m_received_data[idx] = static_cast<char>(mqtt_data);
        return true;
    }
    return false;
}

bool
DataManager::set_data_terminator(size_t idx)
{
    if(idx < MAX_DATA_COUNT - 1)
    {
        m_received_data[idx] = '/0';
        return true;
    }
    return false;
}

void
DataManager::parse_data()
{
    static bool const retained   = false;
    static bool const dup        = false;
    static int const publish_Qos = 1;

    char message[100];
    switch(m_data_indicator)
    {
        case 't':
        {
            if(m_data_count[idx_to_int(IDX::TEMPERATURE)] < MAX_DATA_COUNT)
            {
                m_raw_data.temperature = strtof(m_received_data, nullptr);
                m_sum_data.temperature += m_raw_data.temperature;
                m_data_count[idx_to_int(IDX::TEMPERATURE)]++;

                uint16_t const len = snprintf(message, sizeof(message), "t%f", m_raw_data.temperature);
                m_mqtt_client.beginMessage(DATA_PUBLISH, len, retained, publish_Qos, dup);
                m_mqtt_client.print(message);
                m_mqtt_client.endMessage();
            }
            else
            {
                float const temperature_mean               = m_sum_data.temperature / MAX_DATA_COUNT;
                m_sum_data.temperature                     = 0.0f;
                m_data_count[idx_to_int(IDX::TEMPERATURE)] = 0u;

                uint16_t const len = snprintf(message, sizeof(message), "tm%f", temperature_mean);
                m_mqtt_client.beginMessage(DATA_PUBLISH, len, retained, publish_Qos, dup);
                m_mqtt_client.print(message);
                m_mqtt_client.endMessage();
            }
            break;
        }
        case 'i':
        {
            if(m_data_count[idx_to_int(IDX::INSULATION)] < MAX_DATA_COUNT)
            {
                m_raw_data.insulation = strtof(m_received_data, nullptr);
                m_sum_data.insulation += m_raw_data.insulation;
                m_data_count[idx_to_int(IDX::INSULATION)]++;
            }
            else
            {
                float const insulation_mean               = m_sum_data.insulation / MAX_DATA_COUNT;
                m_sum_data.insulation                     = 0.0f;
                m_data_count[idx_to_int(IDX::INSULATION)] = 0u;
            }
            break;
        }
        case 'w':
        {
            if(m_data_count[idx_to_int(IDX::WIND)] < MAX_DATA_COUNT)
            {
                m_raw_data.wind = strtof(m_received_data, nullptr);
                m_sum_data.wind += m_raw_data.wind;
                m_data_count[idx_to_int(IDX::WIND)]++;
            }
            else
            {
                float const wind_mean               = m_sum_data.wind / MAX_DATA_COUNT;
                m_sum_data.wind                     = 0.0f;
                m_data_count[idx_to_int(IDX::WIND)] = 0u;
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

size_t
DataManager::idx_to_int(IDX idx)
{
    return static_cast<size_t>(idx);
}