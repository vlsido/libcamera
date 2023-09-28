/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2023 Raspberry Pi Ltd
 *
 * hdr.cpp - HDR control algorithm
 */

#include "hdr.h"

#include <libcamera/base/log.h>

#include "../agc_status.h"

using namespace RPiController;
using namespace libcamera;

LOG_DEFINE_CATEGORY(RPiHdr)

#define NAME "rpi.hdr"

void HdrConfig::read(const libcamera::YamlObject &params, const std::string &modeName)
{
	name = modeName;

	if (!params.contains("cadence"))
		LOG(RPiHdr, Fatal) << "No cadence for HDR mode " << name;
	cadence = params["cadence"].getList<unsigned int>().value();
	if (cadence.empty())
		LOG(RPiHdr, Fatal) << "Empty cadence in HDR mode " << name;

	/*
	 * In the JSON file it's easier to use the channel name as the key, but
	 * for us it's convenient to swap them over.
	 */
	for (const auto &[k, v] : params["channel_map"].asDict())
		channelMap[v.get<unsigned int>().value()] = k;
}

Hdr::Hdr(Controller *controller)
	: HdrAlgorithm(controller)
{
}

char const *Hdr::name() const
{
	return NAME;
}

int Hdr::read(const libcamera::YamlObject &params)
{
	/* Make an "HDR off" mode by default so that tuning files don't have to. */
	HdrConfig &offMode = config_["Off"];
	offMode.name = "Off";
	offMode.cadence = { 0 };
	offMode.channelMap[0] = "None";
	status_.mode = offMode.name;

	/*
	 * But we still allow the tuning file to override the "Off" mode if it wants.
	 * For example, maybe an application will make channel 0 be the "short"
	 * channel, in order to apply other AGC controls to it.
	 */
	for (const auto &[key, value] : params.asDict())
		config_[key].read(value, key);

	return 0;
}

int Hdr::setMode(std::string const &mode)
{
	/* Always validate the mode, so it can be used later without checking. */
	auto it = config_.find(mode);
	if (it == config_.end()) {
		LOG(RPiHdr, Warning) << "No such HDR mode " << mode;
		return -1;
	}

	status_.mode = it->second.name;

	return 0;
}

std::vector<unsigned int> Hdr::getChannels() const
{
	return config_.at(status_.mode).cadence;
}

void Hdr::updateAgcStatus(Metadata *metadata)
{
	std::scoped_lock lock(*metadata);
	AgcStatus *agcStatus = metadata->getLocked<AgcStatus>("agc.status");
	if (agcStatus) {
		HdrConfig &hdrConfig = config_[status_.mode];
		auto it = hdrConfig.channelMap.find(agcStatus->channel);
		if (it != hdrConfig.channelMap.end()) {
			status_.channel = it->second;
			agcStatus->hdr = status_;
		} else
			LOG(RPiHdr, Warning) << "Channel " << agcStatus->channel
					     << " not found in mode " << status_.mode;
	} else
		LOG(RPiHdr, Warning) << "No agc.status found";
}

void Hdr::switchMode([[maybe_unused]] CameraMode const &cameraMode, Metadata *metadata)
{
	updateAgcStatus(metadata);
}

void Hdr::process([[maybe_unused]] StatisticsPtr &stats, Metadata *imageMetadata)
{
	updateAgcStatus(imageMetadata);
}

/* Register algorithm with the system. */
static Algorithm *create(Controller *controller)
{
	return (Algorithm *)new Hdr(controller);
}
static RegisterAlgorithm reg(NAME, &create);
