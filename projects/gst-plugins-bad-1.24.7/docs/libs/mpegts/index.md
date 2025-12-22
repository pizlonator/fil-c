# MPEG-TS helper library

This library should be linked to by getting cflags and libs from
gstreamer-plugins-bad-{{ gst_api_version.md }}.pc and adding
-lgstmpegts-{{ gst_api_version.md }} to the library flags.

> NOTE: This library API is considered *unstable*

## Purpose

The MPEG-TS helper library provides a collection of definitions, object,
enumerations and functions to assist with dealing with the base *MPEG 2
Transport Stream* (MPEG-TS) format (as defined by `ISO/IEC 13818-1` and `ITU-T
H.222.0`) and its derivates (`DVB`, `ATSC`, `SCTE`, `ARIB`, `Blu-ray`, `AVCHD`,
...).


This library provides helpers for dealing with:

* The various Section Information (SI) and Program-Specific Information (SI),
  handled with the [GstMpegtsSection](GstMpegtsSection) object and related
  functions.

* The various descriptors present in SI/PSI, handled with the
  [GstMpegtsDescriptor](GstMpegtsDescriptor) object and related functions.


This library does not cover:

* Parsing MPEG-TS packets (PSI or PES) and extracting the sections. One can use
  an existing demuxer/parser element for this, or parse the packets
  themselves.

* Generate and multiplex MPEG-TS packets and sections. One can use an existing
  muxer element for this.

Applications, or external elements, can interact with the existing MPEG-TS
elements via [messages](gst_message_new_mpegts_section) (to receive sections) or
[events](gst_mpegts_section_send_event) (to send sections).

## Specification and References

As much as possible, the information contained in this library is based on the
official Specification and/or References listed below:

### `MPEG-TS`

This is the base specification from which all variants are derived. It covers
the basic sections (for program signalling) and descriptors. All variants must
abide by this specification.

* `ISO/IEC 13818-1` and `ITU-T H.222.0`: *"Information technology – Generic
  coding of moving pictures and associated audio information: Systems"*. The two
  specifications are identical, the ITU one is more easily available (*nudge*).

### `SMPTE-RA` : *SMPTE Registration Authority*
The official registration authority for MPEG-TS. This is used for the base
[Registration Descriptor](gst_mpegts_descriptor_parse_registration) which
allows to unambiguously identify a stream when it is not specified in a standard
(yet).

* <http://smpte-ra.org/>

### `DVB` : *Digital Video Broadcasting*

This standards body covers the variant of MPEG-TS used in Europe, Oceania, and
most of Asia and Africa. The standards are actually published by the `ETSI`
(European Telecommunications Standards Institute).

* `ETSI EN 300 468`: *"Digital Video Broadcasting (DVB); Specification for
  Service Information (SI) in DVB systems"*. Covers all the sections and
  descriptors used in DVB variants.
* `ETSI EN 101 154`: *"Digital Video Broadcasting (DVB);Specification for the
  use of Video and Audio Coding in Broadcast and Broadband
  Applications"*. Provides more details about signalling/sectios for audio/video
  codecs.

### `ATSC` : *Advanced Television Systems Committee*

This set of standards covers the variants of MPEG-TS used in North America.
* `ATSC A/53-3` : *"ATSC Digital Television Standard, Part 3 – Service Multiplex
  and Transport Subsystem Characteristics"*. How ATSC extends the base MPEG-TS.
* `ATSC A/65` : *"ATSC Standard:Program and System Information Protocol for
  Terrestrial Broadcast and Cable"*. Covers all sections and descriptors used in
  ATSC 1.0 variants.
* `ATSC A/90` : *"ATSC Data Broadcast Standard"*. Extensions for data transfer
  (i.e. DSM-CC).
* `ATSC A/107` : *"ATSC 2.0 Standard"*. Adds a few more descriptors.
* `ATSC Code Points Registry` : The list of stream types, decriptor types,
  etc... used by ATSC standards.

### `SCTE` : *Society of Cable Telecommunications Engineers*

This set of standards evolved in parallel with ATSC in North-America. Most of it
has been merged into ATSC and other standards since.

* `ANSI/SCTE 35` : *"Digital Program Insertion Cueing Message for Cable"*

### `DSM-CC` : "Digital Storage Media - Command & Control"

This ISO standard is the base for asynchronously carrying "files" over mpeg-ts.

* `ISO/IEC 13818-6` : *"Information technology — Generic coding of moving
  pictures and associated audio information — Part 6: Extensions for DSM-CC"*.
