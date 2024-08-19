/**
 * Nothrow vector container type.
 * Copyright (C) 2024  dbstream
 *
 * This is an internal header.
 */
#include <VKFW/warn_internal.h>

#ifndef VKFW_VECTOR_H
#define VKFW_VECTOR_H 1

#include <new>
#include <stddef.h>
#include <string.h>

/**
 * This is a vector container optimized for small, POD-type data.
 */
template <class T>
class VKFWvector {
	T *m_data = nullptr;
	size_t m_size = 0;
	size_t m_capacity = 0;

	bool
	grow (size_t size)
	{
		if (m_capacity == size)
			return true;

		if (!size) {
			delete[] m_data;
			m_data = nullptr;
			m_capacity = 0;
			return true;
		}

		T *new_data = new (std::nothrow) T[size];
		if (!new_data)
			return false;

		if (m_data) {
			memcpy (new_data, m_data, (size < m_size) ? size : m_size);
			delete[] m_data;
		}

		m_data = new_data;
		m_capacity = size;
		return true;
	}

public:
	~VKFWvector (void)
	{
		if (m_capacity)
			delete[] m_data;
	}

	bool
	resize (size_t size)
	{
		if (!grow (size))
			return false;
		m_size = size;
		return true;
	}

	bool
	push_back (T elem)
	{
		if (!m_capacity)
			if (!grow (4))
				return false;

		if (m_size >= m_capacity)
			if (!grow (2 * m_capacity))
				return false;

		m_data[m_size++] = elem;
		return true;
	}

	T
	pop_back (void)
	{
		return m_data[--m_size];
	}

	operator T * (void) const
	{
		return m_data;
	}

	const T &
	operator[] (size_t i) const
	{
		return m_data[i];
	}

	T &
	operator[] (size_t i)
	{
		return m_data[i];
	}

	T *
	begin (void)
	{
		return m_data;
	}

	T *
	end (void)
	{
		return m_data + m_size;
	}

	T *
	data (void)
	{
		return m_data;
	}

	size_t
	size (void) const
	{
		return m_size;
	}
};

typedef VKFWvector<char *> VKFWstringvec;

#endif /* VKFW_VECTOR_H */
