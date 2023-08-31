#pragma once

#include <random>
#include <array>
#include <algorithm>

struct DejaVuRandom
{
    std::array<unsigned int, 256> m_state;
    std::minstd_rand0 m_rng;
    int m_loop_index = 0;
    int m_loop_len = 8;
    float m_deja_vu = 0.0;
    std::uniform_real_distribution<float> m_dist{0.0f, 1.0f};
    DejaVuRandom(unsigned int seed) : m_rng(seed)
    {
        for (int i = 0; i < m_state.size(); ++i)
            m_state[i] = m_rng();
    }
    unsigned int max() const { return m_rng.max(); }
    unsigned int min() const { return m_rng.min(); }
    unsigned int operator()() { return next(); }
    unsigned int next()
    {
        auto next = m_state[m_loop_index];
        ++m_loop_index;
        if (m_loop_index >= m_loop_len)
            m_loop_index = 0;
        if (m_dist(m_rng) >= m_deja_vu)
        {
            // rotate state left and generate new random number to end
            std::rotate(m_state.begin(), m_state.begin() + 1, m_state.begin() + m_loop_len);
            m_state[m_loop_len - 1] = m_rng();
        }
        return next;
    }
};
