/* Copyright (c) 2015-2022 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include <memory>
#include "device.hpp"
#include "image.hpp"
#include "buffer.hpp"
#include "command_buffer.hpp"

namespace Granite
{
class FFT
{
public:
	FFT();
	~FFT();

	enum class ResourceType
	{
		Texture,
		Buffer
	};

	enum class Mode
	{
		ForwardComplexToComplex,
		InverseComplexToComplex,
		RealToComplex,
		ComplexToReal
	};

	enum class DataType
	{
		FP32,
		FP16
	};

	struct Options
	{
		unsigned Nx = 1;
		unsigned Ny = 1;
		unsigned Nz = 1;
		ResourceType input_resource = ResourceType::Buffer;
		ResourceType output_resource = ResourceType::Buffer;
		Mode mode = Mode::ForwardComplexToComplex;
		DataType data_type = DataType::FP32;
		unsigned dimensions = 1;
	};

	struct BufferResource
	{
		const Vulkan::Buffer *buffer;
		VkDeviceSize offset;
		VkDeviceSize size;
		uint32_t row_stride;
		uint32_t layer_stride;
	};

	struct ImageResource
	{
		const Vulkan::ImageView *view;
		const Vulkan::Sampler *sampler;
		Vulkan::StockSampler stock_sampler;
		float input_offset[2];
		float input_scale[2];
		int32_t output_offset[2];
	};

	union Resource
	{
		BufferResource buffer;
		ImageResource image;
	};

	bool plan(Vulkan::Device *device, const Options &options);
	void execute(Vulkan::CommandBuffer &cmd, const Resource &dst, const Resource &src);
	void release();

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};
}
