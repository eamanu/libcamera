/* SPDX-License-Identifier: LGPL-2.1-or-later */
/*
 * Copyright (C) 2018, Google Inc.
 *
 * media_object.cpp - Media device objects: entities, pads and links
 */

#include "media_object.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <vector>

#include <linux/media.h>

#include "log.h"
#include "media_device.h"

/**
 * \file media_object.h
 * \brief Provides a class hierarchy that represents the  media objects exposed
 * by the Linux kernel Media Controller APIs.
 *
 * The abstract MediaObject class represents any Media Controller graph object
 * identified by an id unique in the media device context. It is subclassed by
 * the MediaEntity, MediaPad and MediaLink classes that represent the entities,
 * pads and links respectively. They are populated based on the media graph
 * information exposed by the Linux kernel through the MEDIA_IOC_G_TOPOLOGY
 * ioctl.
 *
 * As the media objects represent their kernel counterpart, information about
 * the properties they expose can be found in the Linux kernel documentation.
 *
 * All media objects are meant to be created and destroyed solely by the
 * MediaDevice and thus have private constructors and destructors.
 */

namespace libcamera {

LOG_DECLARE_CATEGORY(MediaDevice)

/**
 * \class MediaObject
 * \brief Base class for all media objects
 *
 * MediaObject is an abstract base class for all media objects in the
 * media graph. Each object is identified by a reference to the media
 * device it belongs to and a unique id within that media device.
 * This base class provide helpers to media objects to keep track of
 * these identifiers.
 *
 * \sa MediaEntity, MediaPad, MediaLink
 */

/**
 * \fn MediaObject::MediaObject()
 * \brief Construct a MediaObject part of the MediaDevice \a dev,
 * identified by the \a id unique within the device
 * \param[in] dev The media device this object belongs to
 * \param[in] id The media object id
 *
 * The caller shall ensure unicity of the object id in the media device context.
 * This constraint is neither enforced nor checked by the MediaObject.
 */

/**
 * \fn MediaObject::device()
 * \brief Retrieve the media device the media object belongs to
 * \return The MediaDevice
 */

/**
 * \fn MediaObject::id()
 * \brief Retrieve the media object id
 * \return The media object id
 */

/**
 * \var MediaObject::dev_
 * \brief The media device the media object belongs to
 */

/**
 * \var MediaObject::id_
 * \brief The media object id
 */

/**
 * \class MediaLink
 * \brief The MediaLink represents a link between two pads in the media graph.
 *
 * Links are created from the information provided by the Media Controller API
 * in the media_v2_link structure. They reference the source() and sink() pads
 * they connect and track the link status through link flags().
 *
 * Each link is referenced in the link array of both of the pads it connect.
 */

/**
 * \brief Enable or disable a link
 * \param[in] enable True to enable the link, false to disable it
 *
 * Set the status of a link according to the value of \a enable.
 * Links between two pads can be set to the enabled or disabled state freely,
 * unless they're immutable links, whose status cannot be changed.
 * Enabling an immutable link is not considered an error, while trying to
 * disable it is.
 *
 * Enabling a link establishes a data connection between two pads, while
 * disabling it interrupts that connection.
 *
 * \return 0 on success or a negative error code otherwise
 */
int MediaLink::setEnabled(bool enable)
{
	unsigned int flags = enable ? MEDIA_LNK_FL_ENABLED : 0;

	int ret = dev_->setupLink(this, flags);
	if (ret)
		return ret;

	flags_ = flags;

	return 0;
}

/**
 * \brief Construct a MediaLink
 * \param[in] link The media link kernel data
 * \param[in] source The source pad at the origin of the link
 * \param[in] sink The sink pad at the destination of the link
 */
MediaLink::MediaLink(const struct media_v2_link *link, MediaPad *source,
		     MediaPad *sink)
	: MediaObject(source->device(), link->id), source_(source),
	  sink_(sink), flags_(link->flags)
{
}

/**
 * \fn MediaLink::source()
 * \brief Retrieve the link's source pad
 * \return The source pad at the origin of the link
 */

/**
 * \fn MediaLink::sink()
 * \brief Retrieve the link's sink pad
 * \return The sink pad at the destination of the link
 */

/**
 * \fn MediaLink::flags()
 * \brief Retrieve the link's flags
 *
 * Link flags are a bitmask of flags defined by the Media Controller API
 * MEDIA_LNK_FL_* macros.
 *
 * \return The link flags
 */

/**
 * \class MediaPad
 * \brief The MediaPad represents a pad of an entity in the media graph
 *
 * Pads are created from the information provided by the Media Controller API
 * in the media_v2_pad structure. They reference the entity() they belong to.
 *
 * In addition to their graph id, media graph pads are identified by an index
 * unique in the context of the entity the pad belongs to.
 *
 * A pad can be either a 'source' pad or a 'sink' pad. This information is
 * captured in the pad flags().
 *
 * Pads are connected through links. Links originating from a source pad are
 * outbound links, and links arriving at a sink pad are inbound links. Pads
 * reference all the links() that are connected to them.
 */

/**
 * \brief Construct a MediaPad
 * \param[in] pad The media pad kernel data
 * \param[in] entity The entity the pad belongs to
 */
MediaPad::MediaPad(const struct media_v2_pad *pad, MediaEntity *entity)
	: MediaObject(entity->device(), pad->id), index_(pad->index), entity_(entity),
	  flags_(pad->flags)
{
}

MediaPad::~MediaPad()
{
	/*
	 * Don't delete the links as we only borrow the reference owned by
	 * MediaDevice.
	 */
	links_.clear();
}

/**
 * \fn MediaPad::index()
 * \brief Retrieve the pad index
 * \return The 0-based pad index identifying the pad in the context of the
 * entity it belongs to
 */

/**
 * \fn MediaPad::entity()
 * \brief Retrieve the entity the pad belongs to
 * \return The MediaEntity the pad belongs to
 */

/**
 * \fn MediaPad::flags()
 * \brief Retrieve the pad flags
 *
 * Pad flags are a bitmask of flags defined by the Media Controller API
 * MEDIA_PAD_FL_* macros.
 *
 * \return The pad flags
 */

/**
 * \fn MediaPad::links()
 * \brief Retrieve all links in the pad
 * \return A list of links connected to the pad
 */

/**
 * \brief Add a new link to this pad
 * \param[in] link The MediaLink to add
 */
void MediaPad::addLink(MediaLink *link)
{
	links_.push_back(link);
}

/**
 * \class MediaEntity
 * \brief The MediaEntity represents an entity in the media graph
 *
 * Entities are created from the information provided by the Media Controller
 * API in the media_v2_entity structure. They reference the pads() they contain.
 *
 * In addition to their graph id, media graph entities are identified by a
 * name() unique in the media device context. They implement a function() and
 * may expose a deviceNode().
 */

/**
 * \fn MediaEntity::name()
 * \brief Retrieve the entity name
 * \return The entity name
 */

/**
 * \fn MediaEntity::function()
 * \brief Retrieve the entity's main function
 *
 * Media entity functions are expressed using the MEDIA_ENT_F_* macros
 * defined by the Media Controller API.
 *
 * \return The entity's function
 */

/**
 * \fn MediaEntity::flags()
 * \brief Retrieve the entity's flags
 *
 * Media entity flags are expressed using the MEDIA_ENT_FL_* macros
 * defined by the Media Controller API.
 *
 * \return The entity's flags
 */

/**
 * \fn MediaEntity::deviceNode()
 * \brief Retrieve the entity's device node path, if any
 * \return The entity's device node path, or an empty string if it is not set
 * \sa int setDeviceNode()
 */

/**
 * \fn MediaEntity::deviceMajor()
 * \brief Retrieve the major number of the interface associated with the entity
 * \return The interface major number, or 0 if the entity isn't associated with
 * an interface
 */

/**
 * \fn MediaEntity::deviceMinor()
 * \brief Retrieve the minor number of the interface associated with the entity
 * \return The interface minor number, or 0 if the entity isn't associated with
 * an interface
 */

/**
 * \fn MediaEntity::pads()
 * \brief Retrieve all pads of the entity
 * \return The list of the entity's pads
 */

/**
 * \brief Get a pad in this entity by its index
 * \param[in] index The 0-based pad index
 * \return The pad identified by \a index, or nullptr if no such pad exist
 */
const MediaPad *MediaEntity::getPadByIndex(unsigned int index) const
{
	for (MediaPad *p : pads_) {
		if (p->index() == index)
			return p;
	}

	return nullptr;
}

/**
 * \brief Get a pad in this entity by its object id
 * \param[in] id The pad id
 * \return The pad identified by \a id, or nullptr if no such pad exist
 */
const MediaPad *MediaEntity::getPadById(unsigned int id) const
{
	for (MediaPad *p : pads_) {
		if (p->id() == id)
			return p;
	}

	return nullptr;
}

/**
 * \brief Set the path to the device node for the associated interface
 * \param[in] deviceNode The interface device node path associated with this entity
 * \return 0 on success or a negative error code otherwise
 */
int MediaEntity::setDeviceNode(const std::string &deviceNode)
{
	/* Make sure the device node can be accessed. */
	int ret = ::access(deviceNode.c_str(), R_OK | W_OK);
	if (ret < 0) {
		ret = -errno;
		LOG(MediaDevice, Error)
			<< "Device node " << deviceNode << " can't be accessed: "
			<< strerror(-ret);
		return ret;
	}

	deviceNode_ = deviceNode;

	return 0;
}

/**
 * \brief Construct a MediaEntity
 * \param[in] dev The media device this entity belongs to
 * \param[in] entity The media entity kernel data
 * \param[in] major The major number of the entity associated interface
 * \param[in] minor The minor number of the entity associated interface
 */
MediaEntity::MediaEntity(MediaDevice *dev,
			 const struct media_v2_entity *entity,
			 unsigned int major, unsigned int minor)
	: MediaObject(dev, entity->id), name_(entity->name),
	  function_(entity->function), flags_(entity->flags),
	  major_(major), minor_(minor)
{
}

MediaEntity::~MediaEntity()
{
	/*
	 * Don't delete the pads as we only borrow the reference owned by
	 * MediaDevice.
	 */
	pads_.clear();
}

/**
 * \brief Add \a pad to the entity's list of pads
 * \param[in] pad The pad to add to the list
 *
 * This function is meant to add pads to the entity during parsing of the media
 * graph, after the MediaPad objects are constructed and before the MediaDevice
 * is made available externally.
 */
void MediaEntity::addPad(MediaPad *pad)
{
	pads_.push_back(pad);
}

} /* namespace libcamera */
