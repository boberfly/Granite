#ifndef MESHLET_PAYLOAD_DECODE_H_
#define MESHLET_PAYLOAD_DECODE_H_

#extension GL_EXT_shader_explicit_arithmetic_types_int16 : require
#extension GL_KHR_shader_subgroup_arithmetic : require
#extension GL_KHR_shader_subgroup_basic : require
#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_scalar_block_layout : require

#include "meshlet_payload_constants.h"

#ifndef MESHLET_PAYLOAD_NUM_U32_STREAMS
#error "Must define MESHLET_PAYLOAD_NUM_U32_STREAMS before including meshlet_payload_decode.h"
#endif

#ifndef MESHLET_PAYLOAD_LARGE_WORKGROUP
#error "Must define MESHLET_PAYLOAD_LARGE_WORKGROUP"
#endif

#ifndef MESHLET_PAYLOAD_DESCRIPTOR_SET
#error "Must define MESHLET_PAYLOAD_DESCRIPTOR_SET"
#endif

#ifndef MESHLET_PAYLOAD_META_BINDING
#error "Must define MESHLET_PAYLOAD_META_BINDING"
#endif

#ifndef MESHLET_PAYLOAD_STREAM_BINDING
#error "Must define MESHLET_PAYLOAD_STREAM_BINDING"
#endif

#ifndef MESHLET_PAYLOAD_PAYLOAD_BINDING
#error "Must define MESHLET_PAYLOAD_PAYLOAD_BINDING"
#endif

struct MeshletStream
{
	u16vec4 predictor_a;
	u16vec4 predictor_b;
	u8vec4 initial_value;
	uint offset_from_base;
	uint16_t bitplane_meta[MESHLET_PAYLOAD_NUM_CHUNKS];
};

struct MeshletMeta
{
	uint base_vertex_offset;
	uint8_t num_primitives_minus_1;
	uint8_t num_attributes_minus_1;
	uint16_t reserved;
};

layout(set = MESHLET_PAYLOAD_DESCRIPTOR_SET, binding = MESHLET_PAYLOAD_META_BINDING, std430) readonly buffer MeshletMetas
{
	MeshletMeta data[];
} meshlet_metas;

layout(set = MESHLET_PAYLOAD_DESCRIPTOR_SET, binding = MESHLET_PAYLOAD_STREAM_BINDING, std430) readonly buffer MeshletStreams
{
	MeshletStream data[];
} meshlet_streams;

layout(set = MESHLET_PAYLOAD_DESCRIPTOR_SET, binding = MESHLET_PAYLOAD_PAYLOAD_BINDING, std430) readonly buffer Payload
{
	uint data[];
} payload;

shared u8vec4 shared_chunk_bit_counts[MESHLET_PAYLOAD_NUM_U32_STREAMS][MESHLET_PAYLOAD_NUM_CHUNKS];
shared uint shared_chunk_offset[MESHLET_PAYLOAD_NUM_U32_STREAMS][MESHLET_PAYLOAD_NUM_CHUNKS];
#if MESHLET_PAYLOAD_LARGE_WORKGROUP
shared uvec2 chunk_values0[MESHLET_PAYLOAD_NUM_CHUNKS];
shared uvec2 chunk_values1[MESHLET_PAYLOAD_NUM_CHUNKS];
#endif

// Hardcodes wave32 atm. Need fallback.

uvec2 pack_u16vec4_to_uvec2(u16vec4 v)
{
	return uvec2(pack32(v.xy), pack32(v.zw));
}

uint repack_uint(uvec2 v)
{
	u16vec4 v16 = u16vec4(unpack16(v.x), unpack16(v.y));
	return pack32(u8vec4(v16));
}

void meshlet_barrier()
{
#if MESHLET_PAYLOAD_LARGE_WORKGROUP
	barrier();
#else
	subgroupBarrier();
#endif
}

void meshlet_init_workgroup(uint meshlet_index)
{
	int subgroup_lane = int(gl_SubgroupInvocationID);

	for (uint stream_index = gl_SubgroupID; stream_index < MESHLET_PAYLOAD_NUM_U32_STREAMS; stream_index += gl_NumSubgroups)
	{
		// Start by decoding the offset for bitplanes for all u32 streams.
		if (subgroup_lane < MESHLET_PAYLOAD_NUM_CHUNKS)
		{
			uint bitplane_value = uint(meshlet_streams.data[stream_index + MESHLET_PAYLOAD_NUM_U32_STREAMS * meshlet_index].bitplane_meta[subgroup_lane]);
			u16vec4 bit_counts = (u16vec4(bitplane_value) >> u16vec4(0, 4, 8, 12)) & 0xfus;
			u16vec2 bit_counts2 = bit_counts.xy + bit_counts.zw;
			uint total_bits = bit_counts2.x + bit_counts2.y;
			uint offset = meshlet_streams.data[stream_index + MESHLET_PAYLOAD_NUM_U32_STREAMS * meshlet_index].offset_from_base;
			shared_chunk_offset[stream_index][subgroup_lane] = subgroupExclusiveAdd(total_bits) + offset;
			shared_chunk_bit_counts[stream_index][subgroup_lane] = u8vec4(bit_counts);
		}
	}

	meshlet_barrier();
}

uint meshlet_get_linear_index()
{
#if MESHLET_PAYLOAD_LARGE_WORKGROUP
	// Rely on SubgroupInvocationID == LocalInvocationID.x here.
	return gl_WorkGroupSize.x * gl_LocalInvocationID.y + gl_SubgroupInvocationID;
#else
	return gl_SubgroupInvocationID;
#endif
}

// Overlap load with consumption.
// Helps RDNA2 quite a lot here!
#define MESHLET_FETCH_BITPLANES(decoded_value, counts, payload_value, offset) \
	for (int i = 0; i < counts; i++) \
	{ \
		decoded_value |= bitfieldExtract(payload_value, int(gl_SubgroupInvocationID), 1) << i; \
		payload_value = payload.data[++offset]; \
	} \
	decoded_value = bitfieldExtract(int(decoded_value), 0, counts)

// Add some specialized variants.

#define MESHLET_PAYLOAD_DECL_STREAM(unrolled_stream_index, iter) \
	u16vec4 predictor_a##iter = meshlet_streams.data[unrolled_stream_index].predictor_a; \
	u16vec4 predictor_b##iter = meshlet_streams.data[unrolled_stream_index].predictor_b; \
	u8vec4 initial_value_##iter = meshlet_streams.data[unrolled_stream_index].initial_value; \
	uvec2 initial_value##iter = pack_u16vec4_to_uvec2(u16vec4(initial_value_##iter)); \
	uvec4 decoded##iter = ivec4(0)

#define MESHLET_PAYLOAD_PROCESS_CHUNK(stream_index, chunk_id, iter) \
	uint bitplane_offsets##iter = shared_chunk_offset[stream_index][chunk_id]; \
	ivec4 bit_counts##iter = ivec4(shared_chunk_bit_counts[stream_index][chunk_id]); \
	uint value##iter = payload.data[bitplane_offsets##iter]; \
	MESHLET_FETCH_BITPLANES(decoded##iter.x, bit_counts##iter.x, value##iter, bitplane_offsets##iter); \
	MESHLET_FETCH_BITPLANES(decoded##iter.y, bit_counts##iter.y, value##iter, bitplane_offsets##iter); \
	MESHLET_FETCH_BITPLANES(decoded##iter.z, bit_counts##iter.z, value##iter, bitplane_offsets##iter); \
	MESHLET_FETCH_BITPLANES(decoded##iter.w, bit_counts##iter.w, value##iter, bitplane_offsets##iter); \
	uvec2 packed_decoded##iter = pack_u16vec4_to_uvec2(u16vec4(decoded##iter)) & 0xff00ffu; \
	if (linear_index == 0) \
		packed_decoded##iter += initial_value##iter; \
	packed_decoded##iter += pack_u16vec4_to_uvec2((predictor_a##iter + predictor_b##iter * uint16_t(linear_index)) >> 8us); \
	packed_decoded##iter = subgroupInclusiveAdd(packed_decoded##iter)

#if MESHLET_PAYLOAD_LARGE_WORKGROUP
uint meshlet_decode_stream_32_wg256(uint meshlet_index, uint stream_index)
{
	uint unrolled_stream_index = MESHLET_PAYLOAD_NUM_U32_STREAMS * meshlet_index + stream_index;
	uint linear_index = meshlet_get_linear_index();
	uint chunk_id = gl_LocalInvocationID.y;

	MESHLET_PAYLOAD_DECL_STREAM(unrolled_stream_index, 0);
	MESHLET_PAYLOAD_PROCESS_CHUNK(stream_index, chunk_id, 0);

	barrier(); // Resolve WAR hazard from last iteration.
	if (gl_SubgroupInvocationID == MESHLET_PAYLOAD_MAX_ELEMENTS / MESHLET_PAYLOAD_NUM_CHUNKS - 1)
		chunk_values0[chunk_id] = packed_decoded0 & 0xff00ffu;
	barrier();
	if (gl_SubgroupID == 0u && gl_SubgroupInvocationID < gl_WorkGroupSize.y)
		chunk_values0[gl_SubgroupInvocationID] = subgroupInclusiveAdd(chunk_values0[gl_SubgroupInvocationID]);
	barrier();
	if (chunk_id != 0)
		packed_decoded0 += chunk_values0[chunk_id - 1];

	return repack_uint(packed_decoded0);
}

uvec2 meshlet_decode_stream_64_wg256(uint meshlet_index, uint stream_index)
{
	// Dual-pump the computation. VGPR use is quite low either way, so this is fine.
	uint unrolled_stream_index = MESHLET_PAYLOAD_NUM_U32_STREAMS * meshlet_index + stream_index;
	uint linear_index = meshlet_get_linear_index();
	uint chunk_id = gl_LocalInvocationID.y;

	MESHLET_PAYLOAD_DECL_STREAM(unrolled_stream_index, 0);
	MESHLET_PAYLOAD_DECL_STREAM(unrolled_stream_index + 1, 1);
	MESHLET_PAYLOAD_PROCESS_CHUNK(stream_index, chunk_id, 0);
	MESHLET_PAYLOAD_PROCESS_CHUNK(stream_index + 1, chunk_id, 1);

	barrier(); // Resolve WAR hazard from last iteration.
	if (gl_SubgroupInvocationID == gl_SubgroupSize - 1)
	{
		chunk_values0[chunk_id] = packed_decoded0 & 0xff00ffu;
		chunk_values1[chunk_id] = packed_decoded1 & 0xff00ffu;
	}
	barrier();
	if (gl_SubgroupID == 0u && gl_SubgroupInvocationID < gl_WorkGroupSize.y)
		chunk_values0[gl_SubgroupInvocationID] = subgroupInclusiveAdd(chunk_values0[gl_SubgroupInvocationID]);
	else if (gl_SubgroupID == 1u && gl_SubgroupInvocationID < gl_WorkGroupSize.y)
		chunk_values1[gl_SubgroupInvocationID] = subgroupInclusiveAdd(chunk_values1[gl_SubgroupInvocationID]);
	barrier();
	if (chunk_id != 0)
	{
		packed_decoded0 += chunk_values0[chunk_id - 1];
		packed_decoded1 += chunk_values1[chunk_id - 1];
	}

	return uvec2(repack_uint(packed_decoded0), repack_uint(packed_decoded1));
}

#define MESHLET_DECODE_STREAM_32(meshlet_index, stream_index, report_cb) { \
	uint value = meshlet_decode_stream_32_wg256(meshlet_index, stream_index); \
	report_cb(gl_LocalInvocationIndex, value); }

#define MESHLET_DECODE_STREAM_64(meshlet_index, stream_index, report_cb) { \
	uvec2 value = meshlet_decode_stream_64_wg256(meshlet_index, stream_index); \
	report_cb(gl_LocalInvocationIndex, value); }

#else

#endif

#endif