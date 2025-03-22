#include "../include/data_manager.h"
#include <cstdio>
#include <cstdlib>

DataManager::DataManager()
    : m_data_indicator('\0'),
      m_data_count(),
      m_received_data(),
      m_raw_data(),
      m_sum_data(),
      m_message(),
      m_message_len()
{
}

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
    switch(m_data_indicator)
    {
        case 't':
        {
            if(m_data_count[idx_to_int(IDX::TEMPERATURE)] < MAX_DATA_COUNT)
            {
                m_raw_data.temperature = strtof(m_received_data, nullptr);
                m_sum_data.temperature += m_raw_data.temperature;
                m_data_count[idx_to_int(IDX::TEMPERATURE)]++;

                m_message_len = snprintf(m_message, sizeof(m_message), "t%.2f", m_raw_data.temperature);
            }
            else
            {
                float const temperature_mean               = m_sum_data.temperature / MAX_DATA_COUNT;
                m_sum_data.temperature                     = 0.0f;
                m_data_count[idx_to_int(IDX::TEMPERATURE)] = 0u;

                m_message_len = snprintf(m_message, sizeof(m_message), "tm%.2f", temperature_mean);
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

                m_message_len = snprintf(m_message, sizeof(m_message), "i%.2f", m_raw_data.insulation);
            }
            else
            {
                float const insulation_mean               = m_sum_data.insulation / MAX_DATA_COUNT;
                m_sum_data.insulation                     = 0.0f;
                m_data_count[idx_to_int(IDX::INSULATION)] = 0u;

                m_message_len = snprintf(m_message, sizeof(m_message), "im%.2f", insulation_mean);
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

                m_message_len = snprintf(m_message, sizeof(m_message), "w%.2f", m_raw_data.wind);
            }
            else
            {
                float const wind_mean               = m_sum_data.wind / MAX_DATA_COUNT;
                m_sum_data.wind                     = 0.0f;
                m_data_count[idx_to_int(IDX::WIND)] = 0u;

                m_message_len = snprintf(m_message, sizeof(m_message), "wm%.2f", wind_mean);
            }
            break;
        }
        default:
        {
            break;
        }
    }
}

char const* const
DataManager::get_message() const
{
    return m_message;
}

size_t
DataManager::get_message_len() const
{
    return m_message_len;
}

size_t
DataManager::idx_to_int(IDX idx)
{
    return static_cast<size_t>(idx);
}