/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2019, Google Inc.
 *
 * request.cpp - Capture request handling
 */

#include <libcamera/request.h>

#include <map>

#include <libcamera/buffer.h>
#include <libcamera/camera.h>
#include <libcamera/stream.h>

#include "log.h"

/**
 * \file request.h
 * \brief Describes a frame capture request to be processed by a camera
 */

namespace libcamera {

LOG_DEFINE_CATEGORY(Request)

/**
 * \enum Request::Status
 * Request completion status
 * \var Request::RequestPending
 * The request hasn't completed yet
 * \var Request::RequestComplete
 * The request has completed
 * \var Request::RequestCancelled
 * The request has been cancelled due to capture stop
 */

/**
 * \class Request
 * \brief A frame capture request
 *
 * A Request allows an application to associate buffers and controls on a
 * per-frame basis to be queued to the camera device for processing.
 */

/**
 * \brief Create a capture request for a camera
 * \param[in] camera The camera that creates the request
 * \param[in] cookie Opaque cookie for application use
 *
 * The \a cookie is stored in the request and is accessible through the
 * cookie() method at any time. It is typically used by applications to map the
 * request to an external resource in the request completion handler, and is
 * completely opaque to libcamera.
 *
 */
Request::Request(Camera *camera, uint64_t cookie)
	: camera_(camera), controls_(camera), cookie_(cookie),
	  status_(RequestPending), cancelled_(false)
{
}

Request::~Request()
{
	for (auto it : bufferMap_) {
		Buffer *buffer = it.second;
		delete buffer;
	}
}

/**
 * \fn Request::controls()
 * \brief Retrieve the request's ControlList
 *
 * Requests store a list of controls to be applied to all frames captured for
 * the request. They are created with an empty list of controls that can be
 * accessed through this method and updated with ControlList::operator[]() or
 * ControlList::update().
 *
 * Only controls supported by the camera to which this request will be
 * submitted shall be included in the controls list. Attempting to add an
 * unsupported control causes undefined behaviour.
 *
 * \return A reference to the ControlList in this request
 */

/**
 * \fn Request::buffers()
 * \brief Retrieve the request's streams to buffers map
 *
 * Return a reference to the map that associates each Stream part of the
 * request to the Buffer the Stream output should be directed to.
 *
 * \return The map of Stream to Buffer
 */

/**
 * \brief Store a Buffer with its associated Stream in the Request
 * \param[in] buffer The Buffer to store in the request
 *
 * Ownership of the buffer is passed to the request. It will be deleted when
 * the request is destroyed after completing.
 *
 * A request can only contain one buffer per stream. If a buffer has already
 * been added to the request for the same stream, this method returns -EEXIST.
 *
 * \return 0 on success or a negative error code otherwise
 * \retval -EEXIST The request already contains a buffer for the stream
 * \retval -EINVAL The buffer does not reference a valid Stream
 */
int Request::addBuffer(std::unique_ptr<Buffer> buffer)
{
	Stream *stream = buffer->stream();
	if (!stream) {
		LOG(Request, Error) << "Invalid stream reference";
		return -EINVAL;
	}

	auto it = bufferMap_.find(stream);
	if (it != bufferMap_.end()) {
		LOG(Request, Error) << "Buffer already set for stream";
		return -EEXIST;
	}

	bufferMap_[stream] = buffer.release();

	return 0;
}

/**
 * \var Request::bufferMap_
 * \brief Mapping of streams to buffers for this request
 *
 * The bufferMap_ tracks the buffers associated with each stream. If a stream is
 * not utilised in this request there will be no buffer for that stream in the
 * map.
 */

/**
 * \brief Return the buffer associated with a stream
 * \param[in] stream The stream the buffer is associated to
 * \return The buffer associated with the stream, or nullptr if the stream is
 * not part of this request
 */
Buffer *Request::findBuffer(Stream *stream) const
{
	auto it = bufferMap_.find(stream);
	if (it == bufferMap_.end())
		return nullptr;

	return it->second;
}

/**
 * \fn Request::cookie()
 * \brief Retrieve the cookie set when the request was created
 * \return The request cookie
 */

/**
 * \fn Request::status()
 * \brief Retrieve the request completion status
 *
 * The request status indicates whether the request has completed successfully
 * or with an error. When requests are created and before they complete the
 * request status is set to RequestPending, and is updated at completion time
 * to RequestComplete. If a request is cancelled at capture stop before it has
 * completed, its status is set to RequestCancelled.
 *
 * \return The request completion status
 */

/**
 * \fn Request::hasPendingBuffers()
 * \brief Check if a request has buffers yet to be completed
 *
 * \return True if the request has buffers pending for completion, false
 * otherwise
 */

/**
 * \brief Validate the request and prepare it for the completion handler
 *
 * Requests that contain no buffers are invalid and are rejected.
 *
 * \return 0 on success or a negative error code otherwise
 * \retval -EINVAL The request is invalid
 */
int Request::prepare()
{
	if (bufferMap_.empty()) {
		LOG(Request, Error) << "Invalid request due to missing buffers";
		return -EINVAL;
	}

	for (auto const &pair : bufferMap_) {
		Buffer *buffer = pair.second;
		buffer->setRequest(this);
		pending_.insert(buffer);
	}

	return 0;
}

/**
 * \brief Complete a queued request
 *
 * Mark the request as complete by updating its status to RequestComplete,
 * unless buffers have been cancelled in which case the status is set to
 * RequestCancelled.
 */
void Request::complete()
{
	ASSERT(!hasPendingBuffers());
	status_ = cancelled_ ? RequestCancelled : RequestComplete;
}

/**
 * \brief Complete a buffer for the request
 * \param[in] buffer The buffer that has completed
 *
 * A request tracks the status of all buffers it contains through a set of
 * pending buffers. This function removes the \a buffer from the set to mark it
 * as complete. All buffers associate with the request shall be marked as
 * complete by calling this function once and once only before reporting the
 * request as complete with the complete() method.
 *
 * \return True if all buffers contained in the request have completed, false
 * otherwise
 */
bool Request::completeBuffer(Buffer *buffer)
{
	int ret = pending_.erase(buffer);
	ASSERT(ret == 1);

	buffer->setRequest(nullptr);

	if (buffer->status() == Buffer::BufferCancelled)
		cancelled_ = true;

	return !hasPendingBuffers();
}

} /* namespace libcamera */
