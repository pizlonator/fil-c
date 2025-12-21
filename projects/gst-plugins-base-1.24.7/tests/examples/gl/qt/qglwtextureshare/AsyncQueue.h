/*
 * GStreamer
 * Copyright (C) 2009 Andrey Nechypurenko <andreynech@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __ASYNCQUEUE_H
#define __ASYNCQUEUE_H

#include <QMutex>
#include <QWaitCondition>
#include <QList>

/**
 * This is the thread safe implementation of the Queue.  It can be
 * used in classical producers/consumers multithreaded scenario.  The
 * template parameter is the class which can be put/get to/from the
 * queue.
 */
template<class T>
class AsyncQueue
{
public:
    AsyncQueue() : waitingReaders(0) {}

    int size()
    {
        QMutexLocker locker(&mutex);
        return this->buffer.size();
    }

    void put(const T& item)
    {
        QMutexLocker locker(&mutex);
        this->buffer.push_back(item);
        if(this->waitingReaders)
            this->bufferIsNotEmpty.wakeOne();
    }

    T get()
    {
        QMutexLocker locker(&mutex);
        while(this->buffer.size() == 0)
        {
            ++(this->waitingReaders);
            this->bufferIsNotEmpty.wait(&mutex);
            --(this->waitingReaders);
        }
        T item = this->buffer.front();
        this->buffer.pop_front();
        return item;
    }

private:
    typedef QList<T> Container;
    QMutex mutex;
    QWaitCondition bufferIsNotEmpty;
    Container buffer;
    short waitingReaders;
};


#endif // __ASYNCQUEUE_H
