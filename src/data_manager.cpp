#include "../include/data_manager.h"
#include <cctype>
#include <cstdio>
#include <cstdlib>

DataManager::DataManager()
    : m_idx_data(), m_received_data(), m_data_count(), m_raw_data(), m_sum_data(), m_message(), m_message_len()
{
}

bool
DataManager::receive_idx_data(char mqtt_data, size_t idx)
{
    static uint8_t const max_idx_data_count = MAX_DATA_COUNT / 10u;
    if(idx < max_idx_data_count - 1u)
    {
        m_idx_data[idx] = mqtt_data;
        return true;
    }
    return false;
}

bool
DataManager::set_idx_data_terminator(size_t idx)
{
    static uint8_t const max_idx_data_count = MAX_DATA_COUNT / 10u;
    if(idx < max_idx_data_count - 1u)
    {
        m_idx_data[idx] = '\0';
        return true;
    }
    return false;
}

bool
DataManager::receive_data(char mqtt_data, size_t idx)
{
    if(idx < MAX_DATA_COUNT - 1)
    {
        m_received_data[idx] = mqtt_data;
        return true;
    }
    return false;
}

bool
DataManager::set_data_terminator(size_t idx)
{
    if(idx < MAX_DATA_COUNT - 1)
    {
        m_received_data[idx] = '\0';
        return true;
    }
    return false;
}

void
DataManager::parse_data()
{
    uint8_t const region_idx = atoi(m_idx_data);
    size_t idx               = 0;

    while(m_received_data[idx] != '\0')
    {
        if(!isdigit(m_received_data[idx]))
        {
            char const data_indicator = m_received_data[idx++];
            float const value         = strtof(&m_received_data[idx], nullptr);

            switch(data_indicator)
            {
                case 't':
                {
                    m_raw_data[region_idx].temperature = value;
                    m_sum_data[region_idx].temperature += m_raw_data[region_idx].temperature;
                    break;
                }
                case 'i':
                {
                    m_raw_data[region_idx].insulation = value;
                    m_sum_data[region_idx].insulation += m_raw_data[region_idx].insulation;
                    break;
                }
                case 'w':
                {
                    m_raw_data[region_idx].wind = value;
                    m_sum_data[region_idx].wind += m_raw_data[region_idx].wind;
                    break;
                }
                default:
                {
                    break;
                }
            }

            while(isdigit(m_received_data[idx]) || m_received_data[idx] == '.' || m_received_data[idx] == '-')
            {
                idx++;
            }
        }
    }

    float temperature_mean = -1.0f;
    float insulation_mean  = -1.0f;
    float wind_mean        = -1.0f;

    if(m_data_count[region_idx] == MEAN_COUNT - 1u)
    {
        temperature_mean = m_sum_data[region_idx].temperature / MEAN_COUNT;
        insulation_mean  = m_sum_data[region_idx].insulation / MEAN_COUNT;
        wind_mean        = m_sum_data[region_idx].wind / MEAN_COUNT;

        m_sum_data[region_idx].temperature = 0.0f;
        m_sum_data[region_idx].insulation  = 0.0f;
        m_sum_data[region_idx].wind        = 0.0f;

        m_data_count[region_idx] = 0u;
    }
    else
    {
        m_data_count[region_idx]++; // Protect against calculating next means from 9 elements instead of 10 (as int the beginning).
    }

    m_message_len = snprintf(
        m_message, sizeof(m_message), "%d_%.2f_%.2f_%.2f_%.2f_%.2f_%.2f", region_idx,
        m_raw_data[region_idx].temperature, m_raw_data[region_idx].insulation, m_raw_data[region_idx].wind,
        temperature_mean, insulation_mean, wind_mean);  
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