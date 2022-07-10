/* Copyright (c) 2017-2022 Hans-Kristian Arntzen
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once

#include "vulkan_headers.hpp"
#include <vector>

namespace Vulkan
{
class Device;
class SemaphoreManager
{
public:
	void init(Device *device);
	~SemaphoreManager();

	VkSemaphore request_cleared_semaphore(bool external);
	void recycle(VkSemaphore semaphore, bool external);

private:
	Device *device = nullptr;
	const VolkDeviceTable *table = nullptr;
	std::vector<VkSemaphore> semaphores;
	std::vector<VkSemaphore> semaphores_external;

	VkExternalSemaphoreHandleTypeFlags importable_types = 0;
	VkExternalSemaphoreHandleTypeFlags exportable_types = 0;
	void test_external_semaphore_handle_type(VkExternalSemaphoreHandleTypeFlagBits handle_type);
};
}
