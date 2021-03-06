/**
 * @file mp4_demux.c
 * @brief MP4 file library - demuxer implementation
 * @date 07/11/2016
 * @author aurelien.barre@akaaba.net
 *
 * Copyright (c) 2016 Aurelien Barre <aurelien.barre@akaaba.net>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of the copyright holder nor the names of the
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#ifdef _WIN32
#  include <winsock2.h>
#else /* !_WIN32 */
#  include <arpa/inet.h>
#endif /* !_WIN32 */

#include <libmp4.h>

#include "mp4_log.h"


#define MP4_UUID                            0x75756964 /* "uuid" */
#define MP4_FILE_TYPE_BOX                   0x66747970 /* "ftyp" */
#define MP4_MOVIE_BOX                       0x6d6f6f76 /* "moov" */
#define MP4_USER_DATA_BOX                   0x75647461 /* "udta" */
#define MP4_MOVIE_HEADER_BOX                0x6d766864 /* "mvhd" */
#define MP4_TRACK_BOX                       0x7472616b /* "trak" */
#define MP4_TRACK_HEADER_BOX                0x746b6864 /* "tkhd" */
#define MP4_TRACK_REFERENCE_BOX             0x74726566 /* "tref" */
#define MP4_MEDIA_BOX                       0x6d646961 /* "mdia" */
#define MP4_MEDIA_HEADER_BOX                0x6d646864 /* "mdhd" */
#define MP4_HANDLER_REFERENCE_BOX           0x68646c72 /* "hdlr" */
#define MP4_MEDIA_INFORMATION_BOX           0x6d696e66 /* "minf" */
#define MP4_VIDEO_MEDIA_HEADER_BOX          0x766d6864 /* "vmhd" */
#define MP4_SOUND_MEDIA_HEADER_BOX          0x736d6864 /* "smhd" */
#define MP4_HINT_MEDIA_HEADER_BOX           0x686d6864 /* "hmhd" */
#define MP4_NULL_MEDIA_HEADER_BOX           0x6e6d6864 /* "nmhd" */
#define MP4_DATA_INFORMATION_BOX            0x64696e66 /* "dinf" */
#define MP4_DATA_REFERENCE_BOX              0x64696566 /* "dref" */ /*TODO*/
#define MP4_SAMPLE_TABLE_BOX                0x7374626c /* "stbl" */
#define MP4_SAMPLE_DESCRIPTION_BOX          0x73747364 /* "stsd" */
#define MP4_AVC_DECODER_CONFIG_BOX          0x61766343 /* "avcC" */
#define MP4_DECODING_TIME_TO_SAMPLE_BOX     0x73747473 /* "stts" */
#define MP4_SYNC_SAMPLE_BOX                 0x73747373 /* "stss" */
#define MP4_SAMPLE_SIZE_BOX                 0x7374737a /* "stsz" */
#define MP4_SAMPLE_TO_CHUNK_BOX             0x73747363 /* "stsc" */
#define MP4_CHUNK_OFFSET_BOX                0x7374636f /* "stco" */
#define MP4_CHUNK_OFFSET_64_BOX             0x636f3634 /* "co64" */
#define MP4_META_BOX                        0x6d657461 /* "meta" */
#define MP4_KEYS_BOX                        0x6b657973 /* "keys" */
#define MP4_ILST_BOX                        0x696c7374 /* "ilst" */
#define MP4_DATA_BOX                        0x64617461 /* "data" */
#define MP4_LOCATION_BOX                    0xa978797a /* ".xyz" */

#define MP4_HANDLER_TYPE_VIDEO              0x76696465 /* "vide" */
#define MP4_HANDLER_TYPE_AUDIO              0x736f756e /* "soun" */
#define MP4_HANDLER_TYPE_HINT               0x68696e74 /* "hint" */
#define MP4_HANDLER_TYPE_METADATA           0x6d657461 /* "meta" */
#define MP4_HANDLER_TYPE_TEXT               0x74657874 /* "text" */

#define MP4_REFERENCE_TYPE_HINT             0x68696e74 /* "hint" */
#define MP4_REFERENCE_TYPE_DESCRIPTION      0x63647363 /* "cdsc" */
#define MP4_REFERENCE_TYPE_HINT_USED        0x68696e64 /* "hind" */
#define MP4_REFERENCE_TYPE_CHAPTERS         0x63686170 /* "chap" */

#define MP4_METADATA_CLASS_UTF8             (1)
#define MP4_METADATA_CLASS_JPEG             (13)
#define MP4_METADATA_CLASS_PNG              (14)
#define MP4_METADATA_CLASS_BMP              (27)

#define MP4_METADATA_TAG_TYPE_ARTIST        0x00415254 /* ".ART" */
#define MP4_METADATA_TAG_TYPE_TITLE         0x006e616d /* ".nam" */
#define MP4_METADATA_TAG_TYPE_DATE          0x00646179 /* ".day" */
#define MP4_METADATA_TAG_TYPE_COMMENT       0x00636d74 /* ".cmt" */
#define MP4_METADATA_TAG_TYPE_COPYRIGHT     0x00637079 /* ".cpy" */
#define MP4_METADATA_TAG_TYPE_MAKER         0x006d616b /* ".mak" */
#define MP4_METADATA_TAG_TYPE_MODEL         0x006d6f64 /* ".mod" */
#define MP4_METADATA_TAG_TYPE_VERSION       0x00737772 /* ".swr" */
#define MP4_METADATA_TAG_TYPE_ENCODER       0x00746f6f /* ".too" */
#define MP4_METADATA_TAG_TYPE_COVER         0x636f7672 /* "covr" */

#define MP4_METADATA_KEY_COVER              "com.apple.quicktime.artwork"

#define MP4_MAC_TO_UNIX_EPOCH_OFFSET (0x7c25b080UL)

#define MP4_CHAPTERS_MAX (100)


struct mp4_box {
	uint32_t size;
	uint32_t type;
	uint64_t largesize;
	uint8_t uuid[16];
};


struct mp4_box_item {
	struct mp4_box_item *parent;
	struct mp4_box_item *child;
	struct mp4_box_item *prev;
	struct mp4_box_item *next;

	struct mp4_box box;
};


struct mp4_time_to_sample_entry {
	uint32_t sampleCount;
	uint32_t sampleDelta;
};


struct mp4_sample_to_chunk_entry {
	uint32_t firstChunk;
	uint32_t samplesPerChunk;
	uint32_t sampleDescriptionIndex;
};


struct mp4_track {
	uint32_t id;
	enum mp4_track_type type;
	uint32_t timescale;
	uint64_t duration;
	uint64_t creationTime;
	uint64_t modificationTime;
	uint32_t currentSample;
	uint32_t sampleCount;
	uint32_t *sampleSize;
	uint64_t *sampleDecodingTime;
	uint64_t *sampleOffset;
	uint32_t chunkCount;
	uint64_t *chunkOffset;
	uint32_t timeToSampleEntryCount;
	struct mp4_time_to_sample_entry *timeToSampleEntries;
	uint32_t sampleToChunkEntryCount;
	struct mp4_sample_to_chunk_entry *sampleToChunkEntries;
	uint32_t syncSampleEntryCount;
	uint32_t *syncSampleEntries;
	uint32_t referenceType;
	uint32_t referenceTrackId;

	enum mp4_video_codec videoCodec;
	uint32_t videoWidth;
	uint32_t videoHeight;
	uint16_t videoSpsSize;
	uint8_t *videoSps;
	uint16_t videoPpsSize;
	uint8_t *videoPps;

	enum mp4_audio_codec audioCodec;
	uint32_t audioChannelCount;
	uint32_t audioSampleSize;
	uint32_t audioSampleRate;

	char *metadataContentEncoding;
	char *metadataMimeFormat;

	struct mp4_track *ref;
	struct mp4_track *metadata;
	struct mp4_track *chapters;

	struct mp4_track *prev;
	struct mp4_track *next;
};


struct mp4_demux {
	FILE *file;
	off_t fileSize;
	off_t readBytes;
	struct mp4_box_item root;
	struct mp4_track *track;
	unsigned int trackCount;
	uint32_t timescale;
	uint64_t duration;
	uint64_t creationTime;
	uint64_t modificationTime;

	char *chaptersName[MP4_CHAPTERS_MAX];
	uint64_t chaptersTime[MP4_CHAPTERS_MAX];
	unsigned int chaptersCount;
	unsigned int finalMetadataCount;
	char **finalMetadataKey;
	char **finalMetadataValue;
	char *udtaLocationKey;
	char *udtaLocationValue;
	off_t finalCoverOffset;
	uint32_t finalCoverSize;
	enum mp4_metadata_cover_type finalCoverType;

	off_t udtaCoverOffset;
	uint32_t udtaCoverSize;
	enum mp4_metadata_cover_type udtaCoverType;
	off_t metaCoverOffset;
	uint32_t metaCoverSize;
	enum mp4_metadata_cover_type metaCoverType;

	unsigned int udtaMetadataCount;
	unsigned int udtaMetadataParseIdx;
	char **udtaMetadataKey;
	char **udtaMetadataValue;
	unsigned int metaMetadataCount;
	char **metaMetadataKey;
	char **metaMetadataValue;
};


#define MP4_READ_32(_file, _val32, _readBytes) \
	do { \
		size_t _count = fread(&_val32, sizeof(uint32_t), 1, _file); \
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((_count == 1), -EIO, \
			"failed to read %zu bytes from file", \
			sizeof(uint32_t)); \
		_readBytes += sizeof(uint32_t); \
	} while (0)

#define MP4_READ_16(_file, _val16, _readBytes) \
	do { \
		size_t _count = fread(&_val16, sizeof(uint16_t), 1, _file); \
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((_count == 1), -EIO, \
			"failed to read %zu bytes from file", \
			sizeof(uint16_t)); \
		_readBytes += sizeof(uint16_t); \
	} while (0)

#define MP4_READ_8(_file, _val8, _readBytes) \
	do { \
		size_t _count = fread(&_val8, sizeof(uint8_t), 1, _file); \
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((_count == 1), -EIO, \
			"failed to read %zu bytes from file", \
			sizeof(uint8_t)); \
		_readBytes += sizeof(uint8_t); \
	} while (0)

#define MP4_SKIP(_file, _readBytes, _maxBytes) \
	do { \
		if (_readBytes < _maxBytes) { \
			int _ret = fseeko(_file, \
				_maxBytes - _readBytes, SEEK_CUR); \
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED( \
				_ret == 0, -errno, \
				"failed to seek %ld bytes forward in file", \
				_maxBytes - _readBytes); \
			_readBytes = _maxBytes; \
		} \
	} while (0)


static off_t mp4_demux_parse_children(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track);


static int mp4_demux_is_sync_sample(
	struct mp4_demux *demux,
	struct mp4_track *track,
	unsigned int sampleIdx,
	int *prevSyncSampleIdx)
{
	unsigned int i;

	if (!track->syncSampleEntries)
		return 1;

	for (i = 0; i < track->syncSampleEntryCount; i++) {
		if (track->syncSampleEntries[i] - 1 == sampleIdx)
			return 1;
		else if (track->syncSampleEntries[i] - 1 > sampleIdx) {
			if ((prevSyncSampleIdx) && (i > 0)) {
				*prevSyncSampleIdx =
					track->syncSampleEntries[i - 1] - 1;
			}
			return 0;
		}
	}

	if ((prevSyncSampleIdx) && (i > 0))
		*prevSyncSampleIdx = track->syncSampleEntries[i - 1] - 1;
	return 0;
}


static off_t mp4_demux_parse_ftyp(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 8);

	/* major_brand */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t majorBrand = ntohl(val32);
	MP4_LOGD("# ftyp: major_brand=%c%c%c%c",
		(char)((majorBrand >> 24) & 0xFF),
		(char)((majorBrand >> 16) & 0xFF),
		(char)((majorBrand >> 8) & 0xFF),
		(char)(majorBrand & 0xFF));

	/* minor_version */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t minorVersion = ntohl(val32);
	MP4_LOGD("# ftyp: minor_version=%" PRIu32, minorVersion);

	int k = 0;
	while (boxReadBytes + 4 <= maxBytes) {
		/* compatible_brands[] */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint32_t compatibleBrands = ntohl(val32);
		MP4_LOGD("# ftyp: compatible_brands[%d]=%c%c%c%c", k,
			(char)((compatibleBrands >> 24) & 0xFF),
			(char)((compatibleBrands >> 16) & 0xFF),
			(char)((compatibleBrands >> 8) & 0xFF),
			(char)(compatibleBrands & 0xFF));
		k++;
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_mvhd(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 25 * 4), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 25 * 4);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# mvhd: version=%d", version);
	MP4_LOGD("# mvhd: flags=%" PRIu32, flags);

	if (version == 1) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 28 * 4),
			-EINVAL, "invalid size: %ld expected %d min",
			maxBytes, 28 * 4);

		/* creation_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->creationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->creationTime |=
			(uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# mvhd: creation_time=%" PRIu64,
			demux->creationTime);

		/* modification_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->modificationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->modificationTime |=
			(uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# mvhd: modification_time=%" PRIu64,
			demux->modificationTime);

		/* timescale */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->timescale = ntohl(val32);
		MP4_LOGD("# mvhd: timescale=%" PRIu32, demux->timescale);

		/* duration */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->duration = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		unsigned int hrs = (unsigned int)(
			(demux->duration + demux->timescale / 2) /
			demux->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(demux->duration + demux->timescale / 2) /
			demux->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(demux->duration + demux->timescale / 2) /
			demux->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# mvhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			demux->duration, hrs, min, sec);
	} else {
		/* creation_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->creationTime = ntohl(val32);
		MP4_LOGD("# mvhd: creation_time=%" PRIu64,
			demux->creationTime);

		/* modification_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->modificationTime = ntohl(val32);
		MP4_LOGD("# mvhd: modification_time=%" PRIu64,
			demux->modificationTime);

		/* timescale */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->timescale = ntohl(val32);
		MP4_LOGD("# mvhd: timescale=%" PRIu32, demux->timescale);

		/* duration */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		demux->duration = ntohl(val32);
		unsigned int hrs = (unsigned int)(
			(demux->duration + demux->timescale / 2) /
			demux->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(demux->duration + demux->timescale / 2) /
			demux->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(demux->duration + demux->timescale / 2) /
			demux->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# mvhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			demux->duration, hrs, min, sec);
	}

	/* rate */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	float rate = (float)ntohl(val32) / 65536.;
	MP4_LOGD("# mvhd: rate=%.4f", rate);

	/* volume & reserved */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	float volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
	MP4_LOGD("# mvhd: volume=%.2f", volume);

	/* reserved */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	MP4_READ_32(demux->file, val32, boxReadBytes);

	/* matrix */
	int k;
	for (k = 0; k < 9; k++)
		MP4_READ_32(demux->file, val32, boxReadBytes);

	/* pre_defined */
	for (k = 0; k < 6; k++)
		MP4_READ_32(demux->file, val32, boxReadBytes);

	/* next_track_ID */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t next_track_ID = ntohl(val32);
	MP4_LOGD("# mvhd: next_track_ID=%" PRIu32, next_track_ID);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_tkhd(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 21 * 4), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 21 * 4);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# tkhd: version=%d", version);
	MP4_LOGD("# tkhd: flags=%" PRIu32, flags);

	if (version == 1) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 24 * 4),
			-EINVAL, "invalid size: %ld expected %d min",
			maxBytes, 24 * 4);

		/* creation_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint64_t creationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# tkhd: creation_time=%" PRIu64,
			creationTime);

		/* modification_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint64_t modificationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		modificationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# tkhd: modification_time=%" PRIu64,
			modificationTime);

		/* track_ID */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->id = ntohl(val32);
		MP4_LOGD("# tkhd: track_ID=%" PRIu32, track->id);

		/* reserved */
		MP4_READ_32(demux->file, val32, boxReadBytes);

		/* duration */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint64_t duration = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		unsigned int hrs = (unsigned int)(
			(duration + demux->timescale / 2) /
			demux->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(duration + demux->timescale / 2) /
			demux->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(duration + demux->timescale / 2) /
			demux->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# tkhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			duration, hrs, min, sec);
	} else {
		/* creation_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint32_t creationTime = ntohl(val32);
		MP4_LOGD("# tkhd: creation_time=%" PRIu32,
			creationTime);

		/* modification_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint32_t modificationTime = ntohl(val32);
		MP4_LOGD("# tkhd: modification_time=%" PRIu32,
			modificationTime);

		/* track_ID */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->id = ntohl(val32);
		MP4_LOGD("# tkhd: track_ID=%" PRIu32, track->id);

		/* reserved */
		MP4_READ_32(demux->file, val32, boxReadBytes);

		/* duration */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint32_t duration = ntohl(val32);
		unsigned int hrs = (unsigned int)(
			(duration + demux->timescale / 2) /
			demux->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(duration + demux->timescale / 2) /
			demux->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(duration + demux->timescale / 2) /
			demux->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# tkhd: duration=%" PRIu32 " (%02d:%02d:%02d)",
			duration, hrs, min, sec);
	}

	/* reserved */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	MP4_READ_32(demux->file, val32, boxReadBytes);

	/* layer & alternate_group */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	int16_t layer = (int16_t)(ntohl(val32) >> 16);
	int16_t alternateGroup = (int16_t)(ntohl(val32) & 0xFFFF);
	MP4_LOGD("# tkhd: layer=%i", layer);
	MP4_LOGD("# tkhd: alternate_group=%i", alternateGroup);

	/* volume & reserved */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	float volume = (float)((ntohl(val32) >> 16) & 0xFFFF) / 256.;
	MP4_LOGD("# tkhd: volume=%.2f", volume);

	/* matrix */
	int k;
	for (k = 0; k < 9; k++)
		MP4_READ_32(demux->file, val32, boxReadBytes);

	/* width */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	float width = (float)ntohl(val32) / 65536.;
	MP4_LOGD("# tkhd: width=%.2f", width);

	/* height */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	float height = (float)ntohl(val32) / 65536.;
	MP4_LOGD("# tkhd: height=%.2f", height);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_tref(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 3 * 4), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 3 * 4);

	/* reference type size */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t referenceTypeSize = ntohl(val32);
	MP4_LOGD("# tref: reference_type_size=%" PRIu32, referenceTypeSize);

	/* reference type */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	track->referenceType = ntohl(val32);
	MP4_LOGD("# tref: reference_type=%c%c%c%c",
		(char)((track->referenceType >> 24) & 0xFF),
		(char)((track->referenceType >> 16) & 0xFF),
		(char)((track->referenceType >> 8) & 0xFF),
		(char)(track->referenceType & 0xFF));

	/* track IDs */
	/* NB: only read the first track ID, ignore multiple references */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	track->referenceTrackId = ntohl(val32);
	MP4_LOGD("# tref: track_id=%" PRIu32, track->referenceTrackId);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_mdhd(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 6 * 4), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 6 * 4);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# mdhd: version=%d", version);
	MP4_LOGD("# mdhd: flags=%" PRIu32, flags);

	if (version == 1) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 9 * 4),
			-EINVAL, "invalid size: %ld expected %d min",
			maxBytes, 9 * 4);

		/* creation_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->creationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->creationTime |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# mdhd: creation_time=%" PRIu64,
			track->creationTime);

		/* modification_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->modificationTime = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->modificationTime |=
			(uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		MP4_LOGD("# mdhd: modification_time=%" PRIu64,
			track->modificationTime);

		/* timescale */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->timescale = ntohl(val32);
		MP4_LOGD("# mdhd: timescale=%" PRIu32, track->timescale);

		/* duration */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->duration = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->duration |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		unsigned int hrs = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# mdhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			track->duration, hrs, min, sec);
	} else {
		/* creation_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->creationTime = ntohl(val32);
		MP4_LOGD("# mdhd: creation_time=%" PRIu64,
			track->creationTime);

		/* modification_time */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->modificationTime = ntohl(val32);
		MP4_LOGD("# mdhd: modification_time=%" PRIu64,
			track->modificationTime);

		/* timescale */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->timescale = ntohl(val32);
		MP4_LOGD("# mdhd: timescale=%" PRIu32, track->timescale);

		/* duration */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->duration = (uint64_t)ntohl(val32);
		unsigned int hrs = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale / 60 / 60);
		unsigned int min = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale / 60 - hrs * 60);
		unsigned int sec = (unsigned int)(
			(track->duration + track->timescale / 2) /
			track->timescale - hrs * 60 * 60 - min * 60);
		MP4_LOGD("# mdhd: duration=%" PRIu64 " (%02d:%02d:%02d)",
			track->duration, hrs, min, sec);
	}

	/* language & pre_defined */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint16_t language = (uint16_t)(ntohl(val32) >> 16) & 0x7FFF;
	MP4_LOGD("# mdhd: language=%" PRIu16, language);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_vmhd(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;
	uint16_t val16;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 3 * 4), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 3 * 4);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# vmhd: version=%d", version);
	MP4_LOGD("# vmhd: flags=%" PRIu32, flags);

	/* graphicsmode */
	MP4_READ_16(demux->file, val16, boxReadBytes);
	uint16_t graphicsmode = ntohs(val16);
	MP4_LOGD("# vmhd: graphicsmode=%" PRIu16, graphicsmode);

	/* opcolor */
	uint16_t opcolor[3];
	MP4_READ_16(demux->file, val16, boxReadBytes);
	opcolor[0] = ntohs(val16);
	MP4_READ_16(demux->file, val16, boxReadBytes);
	opcolor[1] = ntohs(val16);
	MP4_READ_16(demux->file, val16, boxReadBytes);
	opcolor[2] = ntohs(val16);
	MP4_LOGD("# vmhd: opcolor=(%" PRIu16 ",%" PRIu16 ",%" PRIu16 ")",
		opcolor[0], opcolor[1], opcolor[2]);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_smhd(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 2 * 4), -EINVAL,
			"invalid size: %ld expected %d min", maxBytes, 2 * 4);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# smhd: version=%d", version);
	MP4_LOGD("# smhd: flags=%" PRIu32, flags);

	/* balance & reserved */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	float balance = (float)(
		(int16_t)((ntohl(val32) >> 16) & 0xFFFF)) / 256.;
	MP4_LOGD("# smhd: balance=%.2f", balance);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_hmhd(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 5 * 4), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 5 * 4);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# hmhd: version=%d", version);
	MP4_LOGD("# hmhd: flags=%" PRIu32, flags);

	/* maxPDUsize & avgPDUsize */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint16_t maxPDUsize = (uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
	uint16_t avgPDUsize = (uint16_t)(ntohl(val32) & 0xFFFF);
	MP4_LOGD("# hmhd: maxPDUsize=%" PRIu16, maxPDUsize);
	MP4_LOGD("# hmhd: avgPDUsize=%" PRIu16, avgPDUsize);

	/* maxbitrate */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t maxbitrate = ntohl(val32);
	MP4_LOGD("# hmhd: maxbitrate=%" PRIu32, maxbitrate);

	/* avgbitrate */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t avgbitrate = ntohl(val32);
	MP4_LOGD("# hmhd: avgbitrate=%" PRIu32, avgbitrate);

	/* reserved */
	MP4_READ_32(demux->file, val32, boxReadBytes);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_nmhd(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 4), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 4);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# nmhd: version=%d", version);
	MP4_LOGD("# nmhd: flags=%" PRIu32, flags);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_hdlr(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 6 * 4), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 6 * 4);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# hdlr: version=%d", version);
	MP4_LOGD("# hdlr: flags=%" PRIu32, flags);

	/* pre_defined */
	MP4_READ_32(demux->file, val32, boxReadBytes);

	/* handler_type */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t handlerType = ntohl(val32);
	MP4_LOGD("# hdlr: handler_type=%c%c%c%c",
		(char)((handlerType >> 24) & 0xFF),
		(char)((handlerType >> 16) & 0xFF),
		(char)((handlerType >> 8) & 0xFF),
		(char)(handlerType & 0xFF));

	if ((track) && (parent) && (parent->parent) &&
		(parent->parent->box.type == MP4_MEDIA_BOX)) {
		switch (handlerType) {
		case MP4_HANDLER_TYPE_VIDEO:
			track->type = MP4_TRACK_TYPE_VIDEO;
			break;
		case MP4_HANDLER_TYPE_AUDIO:
			track->type = MP4_TRACK_TYPE_AUDIO;
			break;
		case MP4_HANDLER_TYPE_HINT:
			track->type = MP4_TRACK_TYPE_HINT;
			break;
		case MP4_HANDLER_TYPE_METADATA:
			track->type = MP4_TRACK_TYPE_METADATA;
			break;
		case MP4_HANDLER_TYPE_TEXT:
			track->type = MP4_TRACK_TYPE_TEXT;
			break;
		default:
			track->type = MP4_TRACK_TYPE_UNKNOWN;
			break;
		}
	}

	/* reserved */
	unsigned int k;
	for (k = 0; k < 3; k++)
		MP4_READ_32(demux->file, val32, boxReadBytes);

	char name[100];
	for (k = 0; (k < sizeof(name) - 1) && (boxReadBytes < maxBytes); k++) {
		MP4_READ_8(demux->file, name[k], boxReadBytes);
		if (name[k] == '\0')
			break;
	}
	name[k + 1] = '\0';
	MP4_LOGD("# hdlr: name=%s", name);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_avcc(
	struct mp4_demux *demux,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0, minBytes = 6;
	uint32_t val32;
	uint16_t val16;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %ld expected %ld min", maxBytes, minBytes);

	/* version & profile & level */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	val32 = htonl(val32);
	uint8_t version = (val32 >> 24) & 0xFF;
	uint8_t profile = (val32 >> 16) & 0xFF;
	uint8_t profile_compat = (val32 >> 8) & 0xFF;
	uint8_t level = val32 & 0xFF;
	MP4_LOGD("# avcC: version=%d", version);
	MP4_LOGD("# avcC: profile=%d", profile);
	MP4_LOGD("# avcC: profile_compat=%d", profile_compat);
	MP4_LOGD("# avcC: level=%d", level);

	/* length_size & sps_count */
	MP4_READ_16(demux->file, val16, boxReadBytes);
	val16 = htons(val16);
	uint8_t lengthSize = ((val16 >> 8) & 0x3) + 1;
	uint8_t spsCount = val16 & 0x1F;
	MP4_LOGD("# avcC: length_size=%d", lengthSize);
	MP4_LOGD("# avcC: sps_count=%d", spsCount);

	minBytes += 2 * spsCount;
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %ld expected %ld min", maxBytes, minBytes);

	int i;
	for (i = 0; i < spsCount; i++) {
		/* sps_length */
		MP4_READ_16(demux->file, val16, boxReadBytes);
		uint16_t spsLength = htons(val16);
		MP4_LOGD("# avcC: sps_length=%" PRIu16, spsLength);

		minBytes += spsLength;
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes),
			-EINVAL, "invalid size: %ld expected %ld min",
			maxBytes, minBytes);

		if ((!track->videoSps) && (spsLength)) {
			/* first SPS found */
			track->videoSpsSize = spsLength;
			track->videoSps = malloc(spsLength);
			MP4_RETURN_ERR_IF_FAILED((track->videoSps != NULL),
				-ENOMEM);
			size_t count = fread(track->videoSps, spsLength,
				1, demux->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %u bytes from file", spsLength);
		} else {
			/* ignore any other SPS */
			int ret = fseeko(demux->file, spsLength, SEEK_CUR);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(ret == 0, -errno,
				"failed to seek %u bytes forward in file",
				spsLength);
		}
		boxReadBytes += spsLength;
	}

	minBytes++;
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %ld expected %ld min", maxBytes, minBytes);

	/* pps_count */
	uint8_t ppsCount;
	MP4_READ_8(demux->file, ppsCount, boxReadBytes);
	MP4_LOGD("# avcC: pps_count=%d", ppsCount);

	minBytes += 2 * ppsCount;
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes), -EINVAL,
		"invalid size: %ld expected %ld min", maxBytes, minBytes);

	for (i = 0; i < ppsCount; i++) {
		/* pps_length */
		MP4_READ_16(demux->file, val16, boxReadBytes);
		uint16_t ppsLength = htons(val16);
		MP4_LOGD("# avcC: pps_length=%" PRIu16, ppsLength);

		minBytes += ppsLength;
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= minBytes),
			-EINVAL, "invalid size: %ld expected %ld min",
			maxBytes, minBytes);

		if ((!track->videoPps) && (ppsLength)) {
			/* first PPS found */
			track->videoPpsSize = ppsLength;
			track->videoPps = malloc(ppsLength);
			MP4_RETURN_ERR_IF_FAILED((track->videoPps != NULL),
				-ENOMEM);
			size_t count = fread(track->videoPps, ppsLength,
				1, demux->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %u bytes from file", ppsLength);
		} else {
			/* ignore any other PPS */
			int ret = fseeko(demux->file, ppsLength, SEEK_CUR);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(ret == 0, -errno,
				"failed to seek %u bytes forward in file",
				ppsLength);
		}
		boxReadBytes += ppsLength;
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_stsd(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;
	uint16_t val16;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 8);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stsd: version=%d", version);
	MP4_LOGD("# stsd: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t entryCount = ntohl(val32);
	MP4_LOGD("# stsd: entry_count=%" PRIu32, entryCount);

	unsigned int i;
	for (i = 0; i < entryCount; i++) {
		switch (track->type) {
		case MP4_TRACK_TYPE_VIDEO:
		{
			MP4_LOGD("# stsd: video handler type");

			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 102),
				-EINVAL, "invalid size: %ld expected %d min",
				maxBytes, 102);

			/* size */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint32_t size = ntohl(val32);
			MP4_LOGD("# stsd: size=%" PRIu32, size);

			/* type */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint32_t type = ntohl(val32);
			MP4_LOGD("# stsd: type=%c%c%c%c",
				(char)((type >> 24) & 0xFF),
				(char)((type >> 16) & 0xFF),
				(char)((type >> 8) & 0xFF),
				(char)(type & 0xFF));

			/* reserved */
			MP4_READ_32(demux->file, val32, boxReadBytes);

			/* reserved & data_reference_index */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint16_t dataReferenceIndex =
				(uint16_t)(ntohl(val32) & 0xFFFF);
			MP4_LOGD("# stsd: data_reference_index=%" PRIu16,
				dataReferenceIndex);

			int k;
			for (k = 0; k < 4; k++) {
				/* pre_defined & reserved */
				MP4_READ_32(demux->file, val32, boxReadBytes);
			}

			/* width & height */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			track->videoWidth = ((ntohl(val32) >> 16) & 0xFFFF);
			track->videoHeight = (ntohl(val32) & 0xFFFF);
			MP4_LOGD("# stsd: width=%" PRIu16, track->videoWidth);
			MP4_LOGD("# stsd: height=%" PRIu16, track->videoHeight);

			/* horizresolution */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			float horizresolution = (float)(ntohl(val32)) / 65536.;
			MP4_LOGD("# stsd: horizresolution=%.2f",
				horizresolution);

			/* vertresolution */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			float vertresolution = (float)(ntohl(val32)) / 65536.;
			MP4_LOGD("# stsd: vertresolution=%.2f",
				vertresolution);

			/* reserved */
			MP4_READ_32(demux->file, val32, boxReadBytes);

			/* frame_count */
			MP4_READ_16(demux->file, val16, boxReadBytes);
			uint16_t frameCount = ntohs(val16);
			MP4_LOGD("# stsd: frame_count=%" PRIu16, frameCount);

			/* compressorname */
			char compressorname[32];
			size_t count = fread(&compressorname,
				sizeof(compressorname), 1, demux->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %zu bytes from file",
				sizeof(compressorname));
			boxReadBytes += sizeof(compressorname);
			MP4_LOGD("# stsd: compressorname=%s", compressorname);

			/* depth & pre_defined */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint16_t depth =
				(uint16_t)((ntohl(val32) >> 16) & 0xFFFF);
			MP4_LOGD("# stsd: depth=%" PRIu16, depth);

			/* codec specific size */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint32_t codecSize = ntohl(val32);
			MP4_LOGD("# stsd: codec_size=%" PRIu32, codecSize);

			/* codec specific */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint32_t codec = ntohl(val32);
			MP4_LOGD("# stsd: codec=%c%c%c%c",
				(char)((codec >> 24) & 0xFF),
				(char)((codec >> 16) & 0xFF),
				(char)((codec >> 8) & 0xFF),
				(char)(codec & 0xFF));

			if (codec == MP4_AVC_DECODER_CONFIG_BOX) {
				track->videoCodec = MP4_VIDEO_CODEC_AVC;
				off_t ret = mp4_demux_parse_avcc(
					demux, maxBytes - boxReadBytes, track);
				if (ret < 0) {
					MP4_LOGE("mp4_demux_parse_avcc() failed"
						" (%ld)", ret);
					return ret;
				}
				boxReadBytes += ret;
			}
			break;
		}
		case MP4_TRACK_TYPE_AUDIO:
		{
			MP4_LOGD("# stsd: audio handler type");

			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 44),
				-EINVAL, "invalid size: %ld expected %d min",
				maxBytes, 44);

			/* size */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint32_t size = ntohl(val32);
			MP4_LOGD("# stsd: size=%" PRIu32, size);

			/* type */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint32_t type = ntohl(val32);
			MP4_LOGD("# stsd: type=%c%c%c%c",
				(char)((type >> 24) & 0xFF),
				(char)((type >> 16) & 0xFF),
				(char)((type >> 8) & 0xFF),
				(char)(type & 0xFF));

			/* reserved */
			MP4_READ_32(demux->file, val32, boxReadBytes);

			/* reserved & data_reference_index */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint16_t dataReferenceIndex =
				(uint16_t)(ntohl(val32) & 0xFFFF);
			MP4_LOGD("# stsd: data_reference_index=%" PRIu16,
				dataReferenceIndex);

			/* reserved */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			MP4_READ_32(demux->file, val32, boxReadBytes);

			/* channelcount & samplesize */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			track->audioChannelCount =
				((ntohl(val32) >> 16) & 0xFFFF);
			track->audioSampleSize = (ntohl(val32) & 0xFFFF);
			MP4_LOGD("# stsd: channelcount=%" PRIu16,
				track->audioChannelCount);
			MP4_LOGD("# stsd: samplesize=%" PRIu16,
				track->audioSampleSize);

			/* reserved */
			MP4_READ_32(demux->file, val32, boxReadBytes);

			/* samplerate */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			track->audioSampleRate = ntohl(val32);
			MP4_LOGD("# stsd: samplerate=%.2f",
				(float)track->audioSampleRate / 65536.);
			break;
		}
		case MP4_TRACK_TYPE_HINT:
			MP4_LOGD("# stsd: hint handler type");
			break;
		case MP4_TRACK_TYPE_METADATA:
		{
			MP4_LOGD("# stsd: metadata handler type");

			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 24),
				-EINVAL, "invalid size: %ld expected %d min",
				maxBytes, 24);

			/* size */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint32_t size = ntohl(val32);
			MP4_LOGD("# stsd: size=%" PRIu32, size);

			/* type */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			uint32_t type = ntohl(val32);
			MP4_LOGD("# stsd: type=%c%c%c%c",
				(char)((type >> 24) & 0xFF),
				(char)((type >> 16) & 0xFF),
				(char)((type >> 8) & 0xFF),
				(char)(type & 0xFF));

			/* reserved */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			MP4_READ_16(demux->file, val16, boxReadBytes);

			/* data_reference_index */
			MP4_READ_16(demux->file, val16, boxReadBytes);
			uint16_t dataReferenceIndex = ntohl(val16);
			MP4_LOGD("# stsd: size=%d", dataReferenceIndex);

			char str[100];
			unsigned int k;
			for (k = 0; (k < sizeof(str) - 1) &&
				(boxReadBytes < maxBytes); k++) {
				MP4_READ_8(demux->file, str[k], boxReadBytes);
				if (str[k] == '\0')
					break;
			}
			str[k + 1] = '\0';
			if (strlen(str) > 0)
				track->metadataContentEncoding = strdup(str);
			MP4_LOGD("# stsd: content_encoding=%s", str);

			for (k = 0; (k < sizeof(str) - 1) &&
				(boxReadBytes < maxBytes); k++) {
				MP4_READ_8(demux->file, str[k], boxReadBytes);
				if (str[k] == '\0')
					break;
			}
			str[k + 1] = '\0';
			if (strlen(str) > 0)
				track->metadataMimeFormat = strdup(str);
			MP4_LOGD("# stsd: mime_format=%s", str);

			break;
		}
		case MP4_TRACK_TYPE_TEXT:
			MP4_LOGD("# stsd: text handler type");
			break;
		default:
			MP4_LOGD("# stsd: unknown handler type");
			break;
		}
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_stts(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(track->timeToSampleEntries == NULL), -EEXIST,
		"time to sample table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 8);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stts: version=%d", version);
	MP4_LOGD("# stts: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	track->timeToSampleEntryCount = ntohl(val32);
	MP4_LOGD("# stts: entry_count=%" PRIu32, track->timeToSampleEntryCount);

	track->timeToSampleEntries = malloc(track->timeToSampleEntryCount *
		sizeof(struct mp4_time_to_sample_entry));
	MP4_RETURN_ERR_IF_FAILED((track->timeToSampleEntries != NULL), -ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= 8 + track->timeToSampleEntryCount * 8), -EINVAL,
		"invalid size: %ld expected %d min",
		maxBytes, 8 + track->timeToSampleEntryCount * 8);

	unsigned int i;
	for (i = 0; i < track->timeToSampleEntryCount; i++) {
		/* sample_count */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->timeToSampleEntries[i].sampleCount = ntohl(val32);

		/* sample_delta */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->timeToSampleEntries[i].sampleDelta = ntohl(val32);
		/* MP4_LOGD("# stts: sample_count=%" PRIu32
			" sample_delta=%" PRIu32,
			track->timeToSampleEntries[i].sampleCount,
			track->timeToSampleEntries[i].sampleDelta);*/
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_stss(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(track->syncSampleEntries == NULL), -EEXIST,
		"sync sample table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 8);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stss: version=%d", version);
	MP4_LOGD("# stss: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	track->syncSampleEntryCount = ntohl(val32);
	MP4_LOGD("# stss: entry_count=%" PRIu32, track->syncSampleEntryCount);

	track->syncSampleEntries = malloc(
		track->syncSampleEntryCount * sizeof(uint32_t));
	MP4_RETURN_ERR_IF_FAILED((track->syncSampleEntries != NULL), -ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= 8 + track->syncSampleEntryCount * 4), -EINVAL,
		"invalid size: %ld expected %d min",
		maxBytes, 8 + track->syncSampleEntryCount * 4);

	unsigned int i;
	for (i = 0; i < track->syncSampleEntryCount; i++) {
		/* sample_number */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->syncSampleEntries[i] = ntohl(val32);
		/*MP4_LOGD("# stss: sample_number=%" PRIu32,
			track->syncSampleEntries[i]);*/
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_stsz(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->sampleSize == NULL),
		-EEXIST, "sample size table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 12), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 12);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stsz: version=%d", version);
	MP4_LOGD("# stsz: flags=%" PRIu32, flags);

	/* sample_size */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t sampleSize = ntohl(val32);
	MP4_LOGD("# stsz: sample_size=%" PRIu32, sampleSize);

	/* sample_count */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	track->sampleCount = ntohl(val32);
	MP4_LOGD("# stsz: sample_count=%" PRIu32, track->sampleCount);

	track->sampleSize = malloc(track->sampleCount * sizeof(uint32_t));
	MP4_RETURN_ERR_IF_FAILED((track->sampleSize != NULL), -ENOMEM);

	if (sampleSize == 0) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
			(maxBytes >= 12 + track->sampleCount * 4), -EINVAL,
			"invalid size: %ld expected %d min",
			maxBytes, 12 + track->sampleCount * 4);

		unsigned int i;
		for (i = 0; i < track->sampleCount; i++) {
			/* entry_size */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			track->sampleSize[i] = ntohl(val32);
			/*MP4_LOGD("# stsz: entry_size=%" PRIu32,
				track->sampleSize[i]);*/
		}
	} else {
		unsigned int i;
		for (i = 0; i < track->sampleCount; i++)
			track->sampleSize[i] = sampleSize;
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_stsc(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(track->sampleToChunkEntries == NULL), -EEXIST,
		"sample to chunk table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 8);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stsc: version=%d", version);
	MP4_LOGD("# stsc: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	track->sampleToChunkEntryCount = ntohl(val32);
	MP4_LOGD("# stsc: entry_count=%" PRIu32,
		track->sampleToChunkEntryCount);

	track->sampleToChunkEntries = malloc(
		track->sampleToChunkEntryCount *
		sizeof(struct mp4_sample_to_chunk_entry));
	MP4_RETURN_ERR_IF_FAILED((track->sampleToChunkEntries != NULL),
		-ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= 8 + track->sampleToChunkEntryCount * 12), -EINVAL,
		"invalid size: %ld expected %d min",
		maxBytes, 8 + track->sampleToChunkEntryCount * 12);

	unsigned int i;
	for (i = 0; i < track->sampleToChunkEntryCount; i++) {
		/* first_chunk */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->sampleToChunkEntries[i].firstChunk = ntohl(val32);
		/*MP4_LOGD("# stsc: first_chunk=%" PRIu32,
			track->sampleToChunkEntries[i].firstChunk);*/

		/* samples_per_chunk */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->sampleToChunkEntries[i].samplesPerChunk = ntohl(val32);
		/*MP4_LOGD("# stsc: samples_per_chunk=%" PRIu32,
			track->sampleToChunkEntries[i].samplesPerChunk);*/

		/* sample_description_index */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->sampleToChunkEntries[i].sampleDescriptionIndex =
			ntohl(val32);
		/*MP4_LOGD("# stsc: sample_description_index=%" PRIu32,
			track->sampleToChunkEntries[i].sampleDescriptionIndex);
			*/
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_stco(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->chunkOffset == NULL),
		-EEXIST, "chunk offset table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 8);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# stco: version=%d", version);
	MP4_LOGD("# stco: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	track->chunkCount = ntohl(val32);
	MP4_LOGD("# stco: entry_count=%" PRIu32, track->chunkCount);

	track->chunkOffset = malloc(track->chunkCount * sizeof(uint64_t));
	MP4_RETURN_ERR_IF_FAILED((track->chunkOffset != NULL), -ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= 8 + track->chunkCount * 4), -EINVAL,
		"invalid size: %ld expected %d min",
		maxBytes, 8 + track->chunkCount * 4);

	unsigned int i;
	for (i = 0; i < track->chunkCount; i++) {
		/* chunk_offset */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->chunkOffset[i] = (uint64_t)ntohl(val32);
		/*MP4_LOGD("# stco: chunk_offset=%" PRIu64,
			track->chunkOffset[i]);*/
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_co64(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track != NULL), -EINVAL,
		"invalid track");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((track->chunkOffset == NULL),
		-EEXIST, "chunk offset table already defined");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 8);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# co64: version=%d", version);
	MP4_LOGD("# co64: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	track->chunkCount = ntohl(val32);
	MP4_LOGD("# co64: entry_count=%" PRIu32, track->chunkCount);

	track->chunkOffset = malloc(track->chunkCount * sizeof(uint64_t));
	MP4_RETURN_ERR_IF_FAILED((track->chunkOffset != NULL), -ENOMEM);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= 8 + track->chunkCount * 8), -EINVAL,
		"invalid size: %ld expected %d min",
		maxBytes, 8 + track->chunkCount * 8);

	unsigned int i;
	for (i = 0; i < track->chunkCount; i++) {
		/* chunk_offset */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->chunkOffset[i] = (uint64_t)ntohl(val32) << 32;
		MP4_READ_32(demux->file, val32, boxReadBytes);
		track->chunkOffset[i] |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		/*MP4_LOGD("# co64: chunk_offset=%" PRIu64,
			track->chunkOffset[i]);*/
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_xyz(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint16_t val16;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((parent != NULL), -EINVAL,
		"invalid parent");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 4), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 4);

	/* location_size */
	MP4_READ_16(demux->file, val16, boxReadBytes);
	uint16_t locationSize = ntohs(val16);
	MP4_LOGD("# xyz: location_size=%d", locationSize);

	/* language_code */
	MP4_READ_16(demux->file, val16, boxReadBytes);
	uint16_t languageCode = ntohs(val16);
	MP4_LOGD("# xyz: language_code=%d", languageCode);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 4 + locationSize),
		-EINVAL, "invalid size: %ld expected %d min",
		maxBytes, 4 + locationSize);

	demux->udtaLocationKey = malloc(5);
	MP4_RETURN_ERR_IF_FAILED((demux->udtaLocationKey != NULL), -ENOMEM);
	demux->udtaLocationKey[0] = ((parent->box.type >> 24) & 0xFF);
	demux->udtaLocationKey[1] = ((parent->box.type >> 16) & 0xFF);
	demux->udtaLocationKey[2] = ((parent->box.type >> 8) & 0xFF);
	demux->udtaLocationKey[3] = (parent->box.type & 0xFF);
	demux->udtaLocationKey[4] = '\0';

	demux->udtaLocationValue = malloc(locationSize + 1);
	MP4_RETURN_ERR_IF_FAILED((demux->udtaLocationValue != NULL), -ENOMEM);
	size_t count = fread(demux->udtaLocationValue, locationSize,
		1, demux->file);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
		"failed to read %u bytes from file", locationSize);
	boxReadBytes += locationSize;
	demux->udtaLocationValue[locationSize] = '\0';
	MP4_LOGD("# xyz: location=%s", demux->udtaLocationValue);

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static int mp4_demux_count_ilst_sub_box(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t totalReadBytes = 0, boxReadBytes = 0;
	off_t originalOffset, realBoxSize;
	uint32_t val32;
	int lastBox = 0, count = 0;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 8);

	originalOffset = ftello(demux->file);

	while ((totalReadBytes + 8 <= maxBytes) && (!lastBox)) {
		boxReadBytes = 0;

		/* box size */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint32_t size = ntohl(val32);

		/* box type */
		MP4_READ_32(demux->file, val32, boxReadBytes);

		if (size == 0) {
			/* box extends to end of file */
			/*TODO*/
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(0, -ENOSYS,
				"size == 0 for list element"
				" is not implemented");
		} else if (size == 1) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
				(maxBytes >= boxReadBytes + 16), -EINVAL,
				"invalid size: %ld expected %ld min",
				maxBytes, boxReadBytes + 16);

			/* large size */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			realBoxSize = (uint64_t)ntohl(val32) << 32;
			MP4_READ_32(demux->file, val32, boxReadBytes);
			realBoxSize |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
		} else
			realBoxSize = size;

		count++;

		/* skip the rest of the box */
		MP4_SKIP(demux->file, boxReadBytes, realBoxSize);
		totalReadBytes += realBoxSize;
	}

	int ret = fseeko(demux->file, -totalReadBytes, SEEK_CUR);
	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(ret == 0, -errno,
		"failed to seek %ld bytes forward in file", -totalReadBytes);

	return count;
}


static off_t mp4_demux_parse_meta_keys(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32, i;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 8), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 8);

	/* version & flags */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t flags = ntohl(val32);
	uint8_t version = (flags >> 24) & 0xFF;
	flags &= ((1 << 24) - 1);
	MP4_LOGD("# keys: version=%d", version);
	MP4_LOGD("# keys: flags=%" PRIu32, flags);

	/* entry_count */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	demux->metaMetadataCount = ntohl(val32);
	MP4_LOGD("# keys: entry_count=%" PRIu32, demux->metaMetadataCount);

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
		(maxBytes >= 4 + demux->metaMetadataCount * 8), -EINVAL,
		"invalid size: %ld expected %d min",
		maxBytes, 4 + demux->metaMetadataCount * 8);

	demux->metaMetadataKey = calloc(
		demux->metaMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((demux->metaMetadataKey != NULL), -ENOMEM);

	demux->metaMetadataValue = calloc(
		demux->metaMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((demux->metaMetadataValue != NULL), -ENOMEM);

	for (i = 0; i < demux->metaMetadataCount; i++) {
		/* key_size */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint32_t keySize = ntohl(val32);
		MP4_LOGD("# keys: key_size=%" PRIu32, keySize);

		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((keySize >= 8), -EINVAL,
			"invalid key size: %" PRIu32 " expected %d min",
			keySize, 8);
		keySize -= 8;

		/* key_namespace */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		uint32_t keyNamespace = ntohl(val32);
		MP4_LOGD("# keys: key_namespace=%c%c%c%c",
			(char)((keyNamespace >> 24) & 0xFF),
			(char)((keyNamespace >> 16) & 0xFF),
			(char)((keyNamespace >> 8) & 0xFF),
			(char)(keyNamespace & 0xFF));

		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
			(maxBytes - boxReadBytes >= keySize), -EINVAL,
			"invalid size: %ld expected %d min",
			maxBytes - boxReadBytes, keySize);

		demux->metaMetadataKey[i] = malloc(keySize + 1);
		MP4_RETURN_ERR_IF_FAILED((demux->metaMetadataKey[i] != NULL),
			-ENOMEM);
		size_t count = fread(demux->metaMetadataKey[i], keySize,
			1, demux->file);
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
			"failed to read %u bytes from file", keySize);
		boxReadBytes += keySize;
		demux->metaMetadataKey[i][keySize] = '\0';
		MP4_LOGD("# keys: key_value[%i]=%s",
			i, demux->metaMetadataKey[i]);
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_meta_data(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t boxReadBytes = 0;
	uint32_t val32;

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((parent->parent != NULL), -EINVAL,
		"invalid parent");

	MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((maxBytes >= 9), -EINVAL,
		"invalid size: %ld expected %d min", maxBytes, 9);

	/* version & class */
	MP4_READ_32(demux->file, val32, boxReadBytes);
	uint32_t clazz = ntohl(val32);
	uint8_t version = (clazz >> 24) & 0xFF;
	clazz &= 0xFF;
	MP4_LOGD("# data: version=%d", version);
	MP4_LOGD("# data: class=%" PRIu32, clazz);

	/* reserved */
	MP4_READ_32(demux->file, val32, boxReadBytes);

	unsigned int valueLen = maxBytes - boxReadBytes;

	if (clazz == MP4_METADATA_CLASS_UTF8) {
		switch (parent->parent->box.type & 0xFFFFFF) {
		case MP4_METADATA_TAG_TYPE_ARTIST:
		case MP4_METADATA_TAG_TYPE_TITLE:
		case MP4_METADATA_TAG_TYPE_DATE:
		case MP4_METADATA_TAG_TYPE_COMMENT:
		case MP4_METADATA_TAG_TYPE_COPYRIGHT:
		case MP4_METADATA_TAG_TYPE_MAKER:
		case MP4_METADATA_TAG_TYPE_MODEL:
		case MP4_METADATA_TAG_TYPE_VERSION:
		case MP4_METADATA_TAG_TYPE_ENCODER:
		{
			uint32_t idx = demux->udtaMetadataParseIdx++;
			demux->udtaMetadataKey[idx] = malloc(5);
			MP4_RETURN_ERR_IF_FAILED(
				(demux->udtaMetadataKey[idx] != NULL), -ENOMEM);
			demux->udtaMetadataKey[idx][0] =
				((parent->parent->box.type >> 24) & 0xFF);
			demux->udtaMetadataKey[idx][1] =
				((parent->parent->box.type >> 16) & 0xFF);
			demux->udtaMetadataKey[idx][2] =
				((parent->parent->box.type >> 8) & 0xFF);
			demux->udtaMetadataKey[idx][3] =
				(parent->parent->box.type & 0xFF);
			demux->udtaMetadataKey[idx][4] = '\0';
			demux->udtaMetadataValue[idx] = malloc(valueLen + 1);
			MP4_RETURN_ERR_IF_FAILED(
				(demux->udtaMetadataValue[idx] != NULL),
				-ENOMEM);
			size_t count = fread(demux->udtaMetadataValue[idx],
				valueLen, 1, demux->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %u bytes from file", valueLen);
			boxReadBytes += valueLen;
			demux->udtaMetadataValue[idx][valueLen] = '\0';
			MP4_LOGD("# data: value[%s]=%s",
				demux->udtaMetadataKey[idx],
				demux->udtaMetadataValue[idx]);
			break;
		}
		default:
		{
			if ((parent->parent->box.type > 0) &&
				(parent->parent->box.type <=
				demux->metaMetadataCount)) {
				uint32_t idx = parent->parent->box.type - 1;
				demux->metaMetadataValue[idx] =
					malloc(valueLen + 1);
				MP4_RETURN_ERR_IF_FAILED(
					(demux->metaMetadataValue[idx] != NULL),
					-ENOMEM);
				size_t count = fread(
					demux->metaMetadataValue[idx], valueLen,
					1, demux->file);
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					(count == 1), -EIO,
					"failed to read %u bytes from file",
					valueLen);
				boxReadBytes += valueLen;
				demux->metaMetadataValue[idx][valueLen] = '\0';
				MP4_LOGD("# data: value[%s]=%s",
					demux->metaMetadataKey[idx],
					demux->metaMetadataValue[idx]);
			}
			break;
		}
		}
	} else if ((clazz == MP4_METADATA_CLASS_JPEG) ||
		(clazz == MP4_METADATA_CLASS_PNG) ||
		(clazz == MP4_METADATA_CLASS_BMP)) {
		uint32_t type = parent->parent->box.type;
		if (type == MP4_METADATA_TAG_TYPE_COVER) {
			demux->udtaCoverOffset = ftello(demux->file);
			demux->udtaCoverSize = valueLen;
			switch (clazz) {
			default:
			case MP4_METADATA_CLASS_JPEG:
				demux->udtaCoverType =
					MP4_METADATA_COVER_TYPE_JPEG;
				break;
			case MP4_METADATA_CLASS_PNG:
				demux->udtaCoverType =
					MP4_METADATA_COVER_TYPE_PNG;
				break;
			case MP4_METADATA_CLASS_BMP:
				demux->udtaCoverType =
					MP4_METADATA_COVER_TYPE_BMP;
				break;
			}
			MP4_LOGD("# data: udta cover offset=0x%lX"
				" size=%d type=%d", demux->udtaCoverOffset,
				demux->udtaCoverSize, demux->udtaCoverType);
		} else if ((type > 0) && (type <= demux->metaMetadataCount) &&
			(!strcmp(demux->metaMetadataKey[type - 1],
				MP4_METADATA_KEY_COVER))) {
			demux->metaCoverOffset = ftello(demux->file);
			demux->metaCoverSize = valueLen;
			switch (clazz) {
			default:
			case MP4_METADATA_CLASS_JPEG:
				demux->metaCoverType =
					MP4_METADATA_COVER_TYPE_JPEG;
				break;
			case MP4_METADATA_CLASS_PNG:
				demux->metaCoverType =
					MP4_METADATA_COVER_TYPE_PNG;
				break;
			case MP4_METADATA_CLASS_BMP:
				demux->metaCoverType =
					MP4_METADATA_COVER_TYPE_BMP;
				break;
			}
			MP4_LOGD("# data: meta cover offset=0x%lX"
				" size=%d type=%d", demux->metaCoverOffset,
				demux->metaCoverSize, demux->metaCoverType);
		}
	}

	/* skip the rest of the box */
	MP4_SKIP(demux->file, boxReadBytes, maxBytes);

	return boxReadBytes;
}


static off_t mp4_demux_parse_children(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	off_t maxBytes,
	struct mp4_track *track)
{
	off_t parentReadBytes = 0;
	int ret = 0, lastBox = 0;
	struct mp4_box_item *prev = NULL;

	while ((!feof(demux->file)) && (!lastBox) &&
		(parentReadBytes + 8 < maxBytes)) {
		off_t boxReadBytes = 0, realBoxSize;
		uint32_t val32;
		struct mp4_box box;
		memset(&box, 0, sizeof(box));

		/* box size */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		box.size = ntohl(val32);

		/* box type */
		MP4_READ_32(demux->file, val32, boxReadBytes);
		box.type = ntohl(val32);
		if ((parent) && (parent->box.type == MP4_ILST_BOX) &&
			(box.type <= demux->metaMetadataCount))
			MP4_LOGD("offset 0x%lX metadata box size %" PRIu32,
				ftello(demux->file), box.size);
		else
			MP4_LOGD("offset 0x%lX box '%c%c%c%c' size %" PRIu32,
				ftello(demux->file),
				(box.type >> 24) & 0xFF,
				(box.type >> 16) & 0xFF,
				(box.type >> 8) & 0xFF,
				box.type & 0xFF, box.size);

		if (box.size == 0) {
			/* box extends to end of file */
			lastBox = 1;
			realBoxSize = demux->fileSize - demux->readBytes;
		} else if (box.size == 1) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
				(maxBytes >= parentReadBytes + 16), -EINVAL,
				"invalid size: %ld expected %ld min",
				maxBytes, parentReadBytes + 16);

			/* large size */
			MP4_READ_32(demux->file, val32, boxReadBytes);
			box.largesize = (uint64_t)ntohl(val32) << 32;
			MP4_READ_32(demux->file, val32, boxReadBytes);
			box.largesize |= (uint64_t)ntohl(val32) & 0xFFFFFFFFULL;
			realBoxSize = box.largesize;
		} else
			realBoxSize = box.size;

		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
			(maxBytes >= parentReadBytes + realBoxSize), -EINVAL,
			"invalid size: %ld expected %ld min",
			maxBytes, parentReadBytes + realBoxSize);

		/* keep the box in the tree */
		struct mp4_box_item *item = malloc(sizeof(*item));
		MP4_RETURN_ERR_IF_FAILED((item != NULL), -ENOMEM);
		memset(item, 0, sizeof(*item));
		memcpy(&item->box, &box, sizeof(box));
		item->parent = parent;
		if (prev == NULL) {
			if (parent)
				parent->child = item;
		} else {
			prev->next = item;
			item->prev = prev;
		}
		prev = item;

		switch (box.type) {
		case MP4_UUID:
		{
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
				((unsigned)(realBoxSize - boxReadBytes) >=
				sizeof(box.uuid)), -EINVAL,
				"invalid size: %ld expected %zu min",
				realBoxSize - boxReadBytes, sizeof(box.uuid));

			/* box extended type */
			size_t count = fread(box.uuid, sizeof(box.uuid),
				1, demux->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %zu bytes from file",
				sizeof(box.uuid));
			boxReadBytes += sizeof(box.uuid);
			break;
		}
		case MP4_MOVIE_BOX:
		case MP4_USER_DATA_BOX:
		case MP4_MEDIA_BOX:
		case MP4_MEDIA_INFORMATION_BOX:
		case MP4_DATA_INFORMATION_BOX:
		case MP4_SAMPLE_TABLE_BOX:
		{
			off_t _ret = mp4_demux_parse_children(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_FILE_TYPE_BOX:
		{
			off_t _ret = mp4_demux_parse_ftyp(
				demux, item, realBoxSize - boxReadBytes);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_MOVIE_HEADER_BOX:
		{
			off_t _ret = mp4_demux_parse_mvhd(
				demux, item, realBoxSize - boxReadBytes);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_TRACK_BOX:
		{
			/* keep the track in the list */
			struct mp4_track *tk = malloc(sizeof(*tk));
			MP4_RETURN_ERR_IF_FAILED((tk != NULL), -ENOMEM);
			memset(tk, 0, sizeof(*tk));
			tk->next = demux->track;
			if (demux->track)
				demux->track->prev = tk;
			demux->track = tk;
			demux->trackCount++;

			off_t _ret = mp4_demux_parse_children(
				demux, item, realBoxSize - boxReadBytes, tk);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_TRACK_HEADER_BOX:
		{
			off_t _ret = mp4_demux_parse_tkhd(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_TRACK_REFERENCE_BOX:
		{
			off_t _ret = mp4_demux_parse_tref(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_HANDLER_REFERENCE_BOX:
		{
			off_t _ret = mp4_demux_parse_hdlr(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_demux_parse_mdhd(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_VIDEO_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_demux_parse_vmhd(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SOUND_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_demux_parse_smhd(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_HINT_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_demux_parse_hmhd(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_NULL_MEDIA_HEADER_BOX:
		{
			off_t _ret = mp4_demux_parse_nmhd(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SAMPLE_DESCRIPTION_BOX:
		{
			off_t _ret = mp4_demux_parse_stsd(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_DECODING_TIME_TO_SAMPLE_BOX:
		{
			off_t _ret = mp4_demux_parse_stts(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SYNC_SAMPLE_BOX:
		{
			off_t _ret = mp4_demux_parse_stss(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SAMPLE_SIZE_BOX:
		{
			off_t _ret = mp4_demux_parse_stsz(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_SAMPLE_TO_CHUNK_BOX:
		{
			off_t _ret = mp4_demux_parse_stsc(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_CHUNK_OFFSET_BOX:
		{
			off_t _ret = mp4_demux_parse_stco(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_CHUNK_OFFSET_64_BOX:
		{
			off_t _ret = mp4_demux_parse_co64(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_META_BOX:
		{
			if ((parent) &&
				(parent->box.type == MP4_USER_DATA_BOX)) {
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					(realBoxSize - boxReadBytes >= 4),
					-EINVAL,
					"invalid size: %ld expected %d min",
					realBoxSize - boxReadBytes, 4);

				/* version & flags */
				MP4_READ_32(demux->file, val32, boxReadBytes);
				uint32_t flags = ntohl(val32);
				uint8_t version = (flags >> 24) & 0xFF;
				flags &= ((1 << 24) - 1);
				MP4_LOGD("# meta: version=%d", version);
				MP4_LOGD("# meta: flags=%" PRIu32, flags);

				off_t _ret = mp4_demux_parse_children(
					demux, item,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			} else if ((parent) &&
				(parent->box.type == MP4_MOVIE_BOX)) {
				off_t _ret = mp4_demux_parse_children(
					demux, item,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			}
			break;
		}
		case MP4_ILST_BOX:
		{
			if ((parent) && (parent->parent) &&
				(parent->parent->box.type ==
				MP4_USER_DATA_BOX)) {
				demux->udtaMetadataCount =
					mp4_demux_count_ilst_sub_box(
					demux, item,
					realBoxSize - boxReadBytes, track);
				if (demux->udtaMetadataCount > 0) {
					char **key =
						calloc(demux->udtaMetadataCount,
						sizeof(char *));
					MP4_RETURN_ERR_IF_FAILED(
						(key != NULL), -ENOMEM);
					demux->udtaMetadataKey = key;

					char **value =
						calloc(demux->udtaMetadataCount,
						sizeof(char *));
					MP4_RETURN_ERR_IF_FAILED(
						(value != NULL), -ENOMEM);
					demux->udtaMetadataValue = value;

					demux->udtaMetadataParseIdx = 0;
				}
			}
			off_t _ret = mp4_demux_parse_children(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_DATA_BOX:
		{
			off_t _ret = mp4_demux_parse_meta_data(
				demux, item, realBoxSize - boxReadBytes, track);
			MP4_RETURN_ERR_IF_FAILED((_ret >= 0), (int)_ret);
			boxReadBytes += _ret;
			break;
		}
		case MP4_LOCATION_BOX:
		{
			if ((parent) &&
				(parent->box.type == MP4_USER_DATA_BOX)) {
				off_t _ret = mp4_demux_parse_xyz(
					demux, item,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			}
			break;
		}
		case MP4_KEYS_BOX:
		{
			if ((parent) && (parent->box.type == MP4_META_BOX)) {
				off_t _ret = mp4_demux_parse_meta_keys(
					demux, item,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			}
			break;
		}
		default:
		{
			if ((parent) && (parent->box.type == MP4_ILST_BOX)) {
				off_t _ret = mp4_demux_parse_children(
					demux, item,
					realBoxSize - boxReadBytes, track);
				MP4_RETURN_ERR_IF_FAILED((_ret >= 0),
					(int)_ret);
				boxReadBytes += _ret;
			}
			break;
		}
		}

		/* skip the rest of the box */
		if (realBoxSize < boxReadBytes) {
			MP4_LOGE("invalid box size %ld (read bytes: %ld)",
				realBoxSize, boxReadBytes);
			ret = -EIO;
			break;
		}
		int _ret = fseeko(demux->file,
			realBoxSize - boxReadBytes, SEEK_CUR);
		if (_ret != 0) {
			MP4_LOGE("failed to seek %ld bytes forward in file",
				realBoxSize - boxReadBytes);
			ret = -EIO;
			break;
		}

		parentReadBytes += realBoxSize;
	}

	return (ret < 0) ? ret : parentReadBytes;
}


static int mp4_demux_build_tracks(
	struct mp4_demux *demux)
{
	struct mp4_track *tk = NULL, *videoTk = NULL;
	struct mp4_track *metaTk = NULL, *chapTk = NULL;
	int videoTrackCount = 0, audioTrackCount = 0, hintTrackCount = 0;
	int metadataTrackCount = 0, textTrackCount = 0;

	for (tk = demux->track; tk; tk = tk->next) {
		unsigned int i, j, k, n;
		uint32_t lastFirstChunk = 1, lastSamplesPerChunk = 0;
		uint32_t chunkCount, sampleCount = 0, chunkIdx;
		uint64_t offsetInChunk;
		for (i = 0; i < tk->sampleToChunkEntryCount; i++) {
			chunkCount = tk->sampleToChunkEntries[i].firstChunk -
				lastFirstChunk;
			sampleCount += chunkCount * lastSamplesPerChunk;
			lastFirstChunk = tk->sampleToChunkEntries[i].firstChunk;
			lastSamplesPerChunk =
				tk->sampleToChunkEntries[i].samplesPerChunk;
		}
		chunkCount = tk->chunkCount - lastFirstChunk + 1;
		sampleCount += chunkCount * lastSamplesPerChunk;

		if (sampleCount != tk->sampleCount) {
			MP4_LOGE("sample count mismatch: %d vs. %d",
				sampleCount, tk->sampleCount);
			return -EPROTO;
		}

		tk->sampleOffset = malloc(sampleCount * sizeof(uint64_t));
		MP4_RETURN_ERR_IF_FAILED((tk->sampleOffset != NULL), -ENOMEM);

		lastFirstChunk = 1;
		lastSamplesPerChunk = 0;
		for (i = 0, n = 0, chunkIdx = 0;
			i < tk->sampleToChunkEntryCount; i++) {
			chunkCount = tk->sampleToChunkEntries[i].firstChunk -
				lastFirstChunk;
			for (j = 0; j < chunkCount; j++, chunkIdx++) {
				for (k = 0, offsetInChunk = 0;
					k < lastSamplesPerChunk; k++, n++) {
					tk->sampleOffset[n] =
						tk->chunkOffset[chunkIdx] +
						offsetInChunk;
					offsetInChunk += tk->sampleSize[n];
				}
			}
			lastFirstChunk =
				tk->sampleToChunkEntries[i].firstChunk;
			lastSamplesPerChunk =
				tk->sampleToChunkEntries[i].samplesPerChunk;
		}
		chunkCount = tk->chunkCount - lastFirstChunk + 1;
		for (j = 0; j < chunkCount; j++, chunkIdx++) {
			for (k = 0, offsetInChunk = 0;
				k < lastSamplesPerChunk; k++, n++) {
				tk->sampleOffset[n] =
					tk->chunkOffset[chunkIdx] +
					offsetInChunk;
				offsetInChunk += tk->sampleSize[n];
			}
		}

		for (i = 0, sampleCount = 0;
			i < tk->timeToSampleEntryCount; i++)
			sampleCount += tk->timeToSampleEntries[i].sampleCount;

		if (sampleCount != tk->sampleCount) {
			MP4_LOGE("sample count mismatch: %d vs. %d",
				sampleCount, tk->sampleCount);
			return -EPROTO;
		}

		tk->sampleDecodingTime = malloc(sampleCount * sizeof(uint64_t));
		MP4_RETURN_ERR_IF_FAILED(
			(tk->sampleDecodingTime != NULL), -ENOMEM);

		uint64_t ts = 0;
		for (i = 0, k = 0; i < tk->timeToSampleEntryCount; i++) {
			for (j = 0; j < tk->timeToSampleEntries[i].sampleCount;
				j++, k++) {
				tk->sampleDecodingTime[k] = ts;
				ts += tk->timeToSampleEntries[i].sampleDelta;
			}
		}

		switch (tk->type) {
		case MP4_TRACK_TYPE_VIDEO:
			videoTrackCount++;
			videoTk = tk;
			break;
		case MP4_TRACK_TYPE_AUDIO:
			audioTrackCount++;
			break;
		case MP4_TRACK_TYPE_HINT:
			hintTrackCount++;
			break;
		case MP4_TRACK_TYPE_METADATA:
			metadataTrackCount++;
			metaTk = tk;
			break;
		case MP4_TRACK_TYPE_TEXT:
			textTrackCount++;
			break;
		default:
			break;
		}

		/* link tracks using track references */
		if ((tk->referenceType != 0) && (tk->referenceTrackId)) {
			struct mp4_track *tkRef;
			int found = 0;
			for (tkRef = demux->track; tkRef; tkRef = tkRef->next) {
				if (tkRef->id == tk->referenceTrackId) {
					found = 1;
					break;
				}
			}
			if (found) {
				if ((tk->referenceType ==
					MP4_REFERENCE_TYPE_DESCRIPTION)
					&& (tk->type ==
					MP4_TRACK_TYPE_METADATA)) {
					tkRef->metadata = tk;
					tk->ref = tkRef;
				} else if ((tk->referenceType ==
					MP4_REFERENCE_TYPE_CHAPTERS) &&
					(tkRef->type == MP4_TRACK_TYPE_TEXT)) {
					tk->chapters = tkRef;
					tkRef->ref = tk;
					tkRef->type = MP4_TRACK_TYPE_CHAPTERS;
					chapTk = tkRef;
				}
			}
		}
	}

	/* workaround: if we have only 1 video track and 1 metadata
	 * track with no track reference, link them anyway */
	if ((videoTrackCount == 1) && (metadataTrackCount == 1)
			&& (audioTrackCount == 0) && (hintTrackCount == 0)
			&& (videoTk) && (metaTk) && (!videoTk->metadata)) {
		videoTk->metadata = metaTk;
		metaTk->ref = videoTk;
	}

	/* build the chapter list */
	if (chapTk) {
		unsigned int i;
		for (i = 0; i < chapTk->sampleCount; i++) {
			unsigned int sampleSize, readBytes = 0;
			uint16_t sz;
			sampleSize = chapTk->sampleSize[i];
			int _ret = fseeko(demux->file,
				chapTk->sampleOffset[i], SEEK_SET);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(_ret == 0, -errno,
				"failed to seek %" PRIu64
				" bytes forward in file",
				chapTk->sampleOffset[i]);
			MP4_READ_16(demux->file, sz, readBytes);
			sz = ntohs(sz);
			if (sz <= sampleSize - readBytes) {
				char *chapName = malloc(sz + 1);
				MP4_RETURN_ERR_IF_FAILED(
					(chapName != NULL), -ENOMEM);
				demux->chaptersName[demux->chaptersCount] =
					chapName;
				size_t count = fread(
					chapName, sz, 1, demux->file);
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					(count == 1), -EIO,
					"failed to read %u bytes from file",
					sz);
				readBytes += sz;
				chapName[sz] = '\0';
				uint64_t chapTime =
					(chapTk->sampleDecodingTime[i] *
					1000000 + chapTk->timescale / 2) /
					chapTk->timescale;
				MP4_LOGD("chapter #%d time=%" PRIu64 " '%s'",
					demux->chaptersCount + 1,
					chapTime, chapName);
				demux->chaptersTime[demux->chaptersCount] =
					chapTime;
				demux->chaptersCount++;
			}
		}
	}

	return 0;
}


static int mp4_demux_build_metadata(
	struct mp4_demux *demux)
{
	unsigned int i, k = 0, metaCount = 0, udtaCount = 0, xyzCount = 0;

	for (i = 0; i < demux->metaMetadataCount; i++) {
		if ((demux->metaMetadataValue[i]) &&
			(strlen(demux->metaMetadataValue[i]) > 0) &&
			(demux->metaMetadataKey[i]) &&
			(strlen(demux->metaMetadataKey[i]) > 0))
			metaCount++;
	}

	for (i = 0; i < demux->udtaMetadataCount; i++) {
		if ((demux->udtaMetadataValue[i]) &&
			(strlen(demux->udtaMetadataValue[i]) > 0) &&
			(demux->udtaMetadataKey[i]) &&
			(strlen(demux->udtaMetadataKey[i]) > 0))
			udtaCount++;
	}

	if ((demux->udtaLocationValue) &&
		(strlen(demux->udtaLocationValue) > 0) &&
		(demux->udtaLocationKey) &&
		(strlen(demux->udtaLocationKey) > 0))
		xyzCount++;

	demux->finalMetadataCount = metaCount + udtaCount + xyzCount;

	demux->finalMetadataKey = calloc(
		demux->finalMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((demux->finalMetadataKey != NULL), -ENOMEM);

	demux->finalMetadataValue = calloc(
		demux->finalMetadataCount, sizeof(char *));
	MP4_RETURN_ERR_IF_FAILED((demux->finalMetadataValue != NULL), -ENOMEM);

	for (i = 0; i < demux->metaMetadataCount; i++) {
		if ((demux->metaMetadataValue[i]) &&
			(strlen(demux->metaMetadataValue[i]) > 0) &&
			(demux->metaMetadataKey[i]) &&
			(strlen(demux->metaMetadataKey[i]) > 0)) {
			demux->finalMetadataKey[k] =
				demux->metaMetadataKey[i];
			demux->finalMetadataValue[k] =
				demux->metaMetadataValue[i];
			k++;
		}
	}

	for (i = 0; i < demux->udtaMetadataCount; i++) {
		if ((demux->udtaMetadataValue[i]) &&
			(strlen(demux->udtaMetadataValue[i]) > 0) &&
			(demux->udtaMetadataKey[i]) &&
			(strlen(demux->udtaMetadataKey[i]) > 0)) {
			demux->finalMetadataKey[k] =
				demux->udtaMetadataKey[i];
			demux->finalMetadataValue[k] =
				demux->udtaMetadataValue[i];
			k++;
		}
	}

	if ((demux->udtaLocationValue) &&
		(strlen(demux->udtaLocationValue) > 0) &&
		(demux->udtaLocationKey) &&
		(strlen(demux->udtaLocationKey) > 0)) {
		demux->finalMetadataKey[k] = demux->udtaLocationKey;
		demux->finalMetadataValue[k] = demux->udtaLocationValue;
		k++;
	}

	if (demux->metaCoverSize > 0) {
		demux->finalCoverSize = demux->metaCoverSize;
		demux->finalCoverOffset = demux->metaCoverOffset;
		demux->finalCoverType = demux->metaCoverType;
	} else if (demux->udtaCoverSize > 0) {
		demux->finalCoverSize = demux->udtaCoverSize;
		demux->finalCoverOffset = demux->udtaCoverOffset;
		demux->finalCoverType = demux->udtaCoverType;
	}

	return 0;
}


static void mp4_demux_print_children(
	struct mp4_demux *demux,
	struct mp4_box_item *parent,
	int level)
{
	struct mp4_box_item *item = NULL;

	for (item = parent->child; item; item = item->next) {
		int k;
		char spaces[101];
		for (k = 0; k < level && k < 50; k++) {
			spaces[k * 2] = ' ';
			spaces[k * 2 + 1] = ' ';
		}
		spaces[k * 2] = '\0';
		char a = (char)((item->box.type >> 24) & 0xFF);
		char b = (char)((item->box.type >> 16) & 0xFF);
		char c = (char)((item->box.type >> 8) & 0xFF);
		char d = (char)(item->box.type & 0xFF);
		MP4_LOGD("%s- %c%c%c%c size %" PRIu64 "\n", spaces,
			(a >= 32) ? a : '.', (b >= 32) ? b : '.',
			(c >= 32) ? c : '.', (d >= 32) ? d : '.',
			(item->box.size == 1) ?
			item->box.largesize : item->box.size);

		if (item->child)
			mp4_demux_print_children(demux, item, level + 1);
	}
}


static void mp4_demux_free_children(
	struct mp4_demux *demux,
	struct mp4_box_item *parent)
{
	struct mp4_box_item *item = NULL, *next = NULL;

	for (item = parent->child; item; item = next) {
		next = item->next;
		if (item->child)
			mp4_demux_free_children(demux, item);
		free(item);
	}
}


static void mp4_demux_free_tracks(
	struct mp4_demux *demux)
{
	struct mp4_track *tk = NULL, *next = NULL;

	for (tk = demux->track; tk; tk = next) {
		next = tk->next;
		free(tk->timeToSampleEntries);
		free(tk->sampleDecodingTime);
		free(tk->sampleSize);
		free(tk->chunkOffset);
		free(tk->sampleToChunkEntries);
		free(tk->sampleOffset);
		free(tk->videoSps);
		free(tk->videoPps);
		free(tk->metadataContentEncoding);
		free(tk->metadataMimeFormat);
		free(tk);
	}
}


struct mp4_demux *mp4_demux_open(
	const char *filename)
{
	int err = 0, ret;
	off_t retBytes;
	struct mp4_demux *demux;

	MP4_RETURN_VAL_IF_FAILED(filename != NULL, -EINVAL, NULL);
	MP4_RETURN_VAL_IF_FAILED(strlen(filename) != 0, -EINVAL, NULL);

	demux = malloc(sizeof(*demux));
	if (demux == NULL) {
		MP4_LOGE("allocation failed");
		err = -ENOMEM;
		goto error;
	}
	memset(demux, 0, sizeof(*demux));

	demux->file = fopen(filename, "rb");
	if (demux->file == NULL) {
		MP4_LOGE("failed to open file '%s'", filename);
		err = -errno;
		goto error;
	}

	ret = fseeko(demux->file, 0, SEEK_END);
	if (ret != 0) {
		MP4_LOGE("failed to seek to end of file");
		err = -errno;
		goto error;
	}
	demux->fileSize = ftello(demux->file);
	ret = fseeko(demux->file, 0, SEEK_SET);
	if (ret != 0) {
		MP4_LOGE("failed to seek to beginning of file");
		err = -errno;
		goto error;
	}

	retBytes = mp4_demux_parse_children(
		demux, &demux->root, demux->fileSize, NULL);
	if (retBytes < 0) {
		MP4_LOGE("mp4_demux_parse_children() failed (%ld)", retBytes);
		err = -EIO;
		goto error;
	} else {
		demux->readBytes += retBytes;
	}

	ret = mp4_demux_build_tracks(demux);
	if (ret < 0) {
		MP4_LOGE("mp4_demux_build_tracks() failed (%d)", ret);
		err = -EIO;
		goto error;
	}
	ret = mp4_demux_build_metadata(demux);
	if (ret < 0) {
		MP4_LOGE("mp4_demux_build_metadata() failed (%d)", ret);
		err = -EIO;
		goto error;
	}

	mp4_demux_print_children(demux, &demux->root, 0);

	return demux;

error:
	if (demux)
		mp4_demux_close(demux);

	MP4_RETURN_VAL_IF_FAILED(1, err, NULL);
	return NULL;
}


int mp4_demux_close(
	struct mp4_demux *demux)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	if (demux) {
		if (demux->file)
			fclose(demux->file);
		mp4_demux_free_children(demux, &demux->root);
		mp4_demux_free_tracks(demux);
		unsigned int i;
		for (i = 0; i < demux->chaptersCount; i++)
			free(demux->chaptersName[i]);
		free(demux->udtaLocationKey);
		free(demux->udtaLocationValue);
		for (i = 0; i < demux->udtaMetadataCount; i++) {
			free(demux->udtaMetadataKey[i]);
			free(demux->udtaMetadataValue[i]);
		}
		free(demux->udtaMetadataKey);
		free(demux->udtaMetadataValue);
		for (i = 0; i < demux->metaMetadataCount; i++) {
			free(demux->metaMetadataKey[i]);
			free(demux->metaMetadataValue[i]);
		}
		free(demux->metaMetadataKey);
		free(demux->metaMetadataValue);
		free(demux->finalMetadataKey);
		free(demux->finalMetadataValue);
	}

	free(demux);

	return 0;
}


int mp4_demux_seek(
	struct mp4_demux *demux,
	uint64_t time_offset,
	int sync)
{
	struct mp4_track *tk = NULL;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	for (tk = demux->track; tk; tk = tk->next) {
		if (tk->type == MP4_TRACK_TYPE_CHAPTERS)
			continue;
		if ((tk->type == MP4_TRACK_TYPE_METADATA) && (tk->ref))
			continue;

		int found = 0, i;
		uint64_t ts = (time_offset * tk->timescale + 500000) / 1000000;
		int start = (unsigned int)(((uint64_t)tk->sampleCount * ts
				+ tk->duration - 1) / tk->duration);
		if (start < 0)
			start = 0;
		if ((unsigned)start >= tk->sampleCount)
			start = tk->sampleCount - 1;
		while (((unsigned)start < tk->sampleCount)
				&& (tk->sampleDecodingTime[start] < ts))
			start++;
		for (i = start; i >= 0; i--) {
			if (tk->sampleDecodingTime[i] <= ts) {
				int isSync, prevSync = -1;
				isSync = mp4_demux_is_sync_sample(
					demux, tk, i, &prevSync);
				if ((isSync) || (!sync)) {
					start = i;
					found = 1;
					break;
				} else if (prevSync >= 0) {
					start = prevSync;
					found = 1;
					break;
				}
			}
		}
		if (found) {
			tk->currentSample = start;
			MP4_LOGI("seek to %" PRIu64
				" -> sample #%d time %" PRIu64,
				time_offset, start,
				(tk->sampleDecodingTime[start] * 1000000 +
				tk->timescale / 2) / tk->timescale);
			if ((tk->metadata) &&
				((unsigned)start < tk->metadata->sampleCount) &&
				(tk->sampleDecodingTime[start] ==
				tk->metadata->sampleDecodingTime[start]))
				tk->metadata->currentSample = start;
			else
				MP4_LOGW("failed to sync metadata"
					" with ref track");
		} else {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOENT,
				"unable to seek in track");
		}
	}

	return 0;
}


int mp4_demux_get_media_info(
	struct mp4_demux *demux,
	struct mp4_media_info *media_info)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(media_info != NULL, -EINVAL);

	memset(media_info, 0, sizeof(*media_info));

	media_info->duration =
		(demux->duration * 1000000 + demux->timescale / 2) /
		demux->timescale;
	media_info->creation_time =
		demux->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	media_info->modification_time =
		demux->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
	media_info->track_count = demux->trackCount;

	return 0;
}


int mp4_demux_get_track_count(
	struct mp4_demux *demux)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	return demux->trackCount;
}


int mp4_demux_get_track_info(
	struct mp4_demux *demux,
	unsigned int track_idx,
	struct mp4_track_info *track_info)
{
	struct mp4_track *tk = NULL;
	unsigned int k;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(track_info != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(track_idx < demux->trackCount, -EINVAL);

	memset(track_info, 0, sizeof(*track_info));

	for (tk = demux->track, k = 0; (tk) && (k < track_idx);
		tk = tk->next, k++)
		;

	if (tk) {
		track_info->id = tk->id;
		track_info->type = tk->type;
		track_info->duration =
			(tk->duration * 1000000 + tk->timescale / 2) /
			tk->timescale;
		track_info->creation_time =
			tk->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
		track_info->modification_time =
			tk->creationTime - MP4_MAC_TO_UNIX_EPOCH_OFFSET;
		track_info->sample_count = tk->sampleCount;
		track_info->has_metadata = (tk->metadata) ? 1 : 0;
		if (tk->metadata) {
			track_info->metadata_content_encoding =
				tk->metadata->metadataContentEncoding;
			track_info->metadata_mime_format =
				tk->metadata->metadataMimeFormat;
		} else if (tk->type == MP4_TRACK_TYPE_METADATA) {
			track_info->metadata_content_encoding =
				tk->metadataContentEncoding;
			track_info->metadata_mime_format =
				tk->metadataMimeFormat;
		}
		if (tk->type == MP4_TRACK_TYPE_VIDEO) {
			track_info->video_codec = tk->videoCodec;
			track_info->video_width = tk->videoWidth;
			track_info->video_height = tk->videoHeight;
		} else if (tk->type == MP4_TRACK_TYPE_AUDIO) {
			track_info->audio_codec = tk->audioCodec;
			track_info->audio_channel_count = tk->audioChannelCount;
			track_info->audio_sample_size = tk->audioSampleSize;
			track_info->audio_sample_rate =
				(float)tk->audioSampleRate / 65536.;
		}
	} else {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOENT,
			"track not found");
	}

	return 0;
}


int mp4_demux_get_track_avc_decoder_config(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint8_t **sps,
	unsigned int *sps_size,
	uint8_t **pps,
	unsigned int *pps_size)
{
	struct mp4_track *tk = NULL;
	int found = 0;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(sps != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(sps_size != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(pps != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(pps_size != NULL, -EINVAL);

	for (tk = demux->track; tk; tk = tk->next) {
		if (tk->id == track_id) {
			found = 1;
			break;
		}
	}

	if (!found) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOENT,
			"track not found");
	}

	if (tk->videoSps) {
		*sps = tk->videoSps;
		*sps_size = tk->videoSpsSize;
	}
	if (tk->videoPps) {
		*pps = tk->videoPps;
		*pps_size = tk->videoPpsSize;
	}

	return 0;
}


int mp4_demux_get_track_next_sample(
	struct mp4_demux *demux,
	unsigned int track_id,
	uint8_t *sample_buffer,
	unsigned int sample_buffer_size,
	uint8_t *metadata_buffer,
	unsigned int metadata_buffer_size,
	struct mp4_track_sample *track_sample)
{
	struct mp4_track *tk = NULL;
	int found = 0;

	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(track_sample != NULL, -EINVAL);

	memset(track_sample, 0, sizeof(*track_sample));

	for (tk = demux->track; tk; tk = tk->next) {
		if (tk->id == track_id) {
			found = 1;
			break;
		}
	}

	if (!found) {
		MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOENT,
			"track not found");
	}

	if (tk->currentSample < tk->sampleCount) {
		track_sample->sample_size = tk->sampleSize[tk->currentSample];
		if ((sample_buffer) &&
			(tk->sampleSize[tk->currentSample] <=
			sample_buffer_size)) {
			int _ret = fseeko(demux->file,
				tk->sampleOffset[tk->currentSample], SEEK_SET);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(_ret == 0, -errno,
				"failed to seek %" PRIu64
				" bytes forward in file",
				tk->sampleOffset[tk->currentSample]);
			size_t count = fread(sample_buffer,
				tk->sampleSize[tk->currentSample],
				1, demux->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %d bytes from file",
				tk->sampleSize[tk->currentSample]);
		} else if (sample_buffer) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOBUFS,
				"buffer too small (%d bytes, %d needed)",
				sample_buffer_size,
				tk->sampleSize[tk->currentSample]);
		}
		if (tk->metadata) {
			struct mp4_track *metatk = tk->metadata;
			/* TODO: check sync between metadata and ref track */
			track_sample->metadata_size =
				metatk->sampleSize[tk->currentSample];
			if ((metadata_buffer) &&
				(metatk->sampleSize[tk->currentSample] <=
				metadata_buffer_size)) {
				int _ret = fseeko(demux->file,
					metatk->sampleOffset[tk->currentSample],
					SEEK_SET);
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					_ret == 0, -errno,
					"failed to seek %" PRIu64
					" bytes forward in file",
					metatk->sampleOffset[
						tk->currentSample]);
				size_t count = fread(metadata_buffer,
					metatk->sampleSize[tk->currentSample],
					1, demux->file);
				MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(
					(count == 1), -EIO,
					"failed to read %d bytes from file",
					metatk->sampleSize[tk->currentSample]);
			}
		}
		track_sample->sample_dts =
			(tk->sampleDecodingTime[tk->currentSample] * 1000000 +
			tk->timescale / 2) / tk->timescale;
		track_sample->next_sample_dts =
			(tk->currentSample < tk->sampleCount - 1) ?
			(tk->sampleDecodingTime[tk->currentSample + 1] *
			1000000 + tk->timescale / 2) / tk->timescale : 0;
		tk->currentSample++;
	}

	return 0;
}


int mp4_demux_get_chapters(
	struct mp4_demux *demux,
	unsigned int *chaptersCount,
	uint64_t **chaptersTime,
	char ***chaptersName)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(chaptersCount != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(chaptersTime != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(chaptersName != NULL, -EINVAL);

	*chaptersCount = demux->chaptersCount;
	*chaptersTime = demux->chaptersTime;
	*chaptersName = demux->chaptersName;

	return 0;
}


int mp4_demux_get_metadata_strings(
	struct mp4_demux *demux,
	unsigned int *count,
	char ***keys,
	char ***values)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(count != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(keys != NULL, -EINVAL);
	MP4_RETURN_ERR_IF_FAILED(values != NULL, -EINVAL);

	*count = demux->finalMetadataCount;
	*keys = demux->finalMetadataKey;
	*values = demux->finalMetadataValue;

	return 0;
}


int mp4_demux_get_metadata_cover(
	struct mp4_demux *demux,
	uint8_t *cover_buffer,
	unsigned int cover_buffer_size,
	unsigned int *cover_size,
	enum mp4_metadata_cover_type *cover_type)
{
	MP4_RETURN_ERR_IF_FAILED(demux != NULL, -EINVAL);

	if (demux->finalCoverSize > 0) {
		if (cover_size)
			*cover_size = demux->finalCoverSize;
		if (cover_type)
			*cover_type = demux->finalCoverType;
		if ((cover_buffer) &&
			(demux->finalCoverSize <= cover_buffer_size)) {
			int _ret = fseeko(demux->file,
				demux->finalCoverOffset, SEEK_SET);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(_ret == 0, -errno,
				"failed to seek %zi bytes forward in file",
				demux->finalCoverOffset);
			size_t count = fread(cover_buffer,
				demux->finalCoverSize, 1, demux->file);
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED((count == 1), -EIO,
				"failed to read %" PRIu32 " bytes from file",
				demux->finalCoverSize);
		} else if (cover_buffer) {
			MP4_LOG_ERR_AND_RETURN_ERR_IF_FAILED(1, -ENOBUFS,
				"buffer too small (%d bytes, %d needed)",
				cover_buffer_size, demux->finalCoverSize);
		}
	} else {
		*cover_size = 0;
	}

	return 0;
}
