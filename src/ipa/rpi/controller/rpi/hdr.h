/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (C) 2023, Raspberry Pi Ltd
 *
 * hdr.h - HDR control algorithm
 */
#pragma once

#include <map>
#include <string>
#include <vector>

#include "../hdr_algorithm.h"
#include "../hdr_status.h"

/* This is our implementation of an HDR algorithm. */

namespace RPiController {

struct HdrConfig {
	std::string name;
	std::vector<unsigned int> cadence;
	std::map<unsigned int, std::string> channelMap;

	void read(const libcamera::YamlObject &params, const std::string &name);
};

class Hdr : public HdrAlgorithm
{
public:
	Hdr(Controller *controller);
	char const *name() const override;
	void switchMode(CameraMode const &cameraMode, Metadata *metadata) override;
	int read(const libcamera::YamlObject &params) override;
	void process(StatisticsPtr &stats, Metadata *imageMetadata) override;
	int setMode(std::string const &mode) override;
	std::vector<unsigned int> getChannels() const override;

private:
	void updateAgcStatus(Metadata *metadata);

	std::map<std::string, HdrConfig> config_;
	HdrStatus status_; /* track the current HDR mode and channel */
};

} /* namespace RPiController */
