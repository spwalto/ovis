.. role:: raw-latex(raw)
   :format: latex
..

." Manpage for Plugin_store_kafka ." Contact ovis-help@ca.sandia.gov to
correct errors or typos. .TH man 7 “2 Jun 2022” “v4” “LDMSD
Decomposition man page”

.SH NAME ldmsd_decomposition - manual for LDMSD decomposition

.SH DESCRIPTION A decomposition is a routine that converts LDMS set into
one or more rows before feeding them to the store. Currently, only
:raw-latex:`\fBstore`\_sos:raw-latex:`\fR`,
:raw-latex:`\fBstore`\_csv:raw-latex:`\fR`, and
:raw-latex:`\fBstore`\_kafka:raw-latex:`\fR `support decomposition. To
use decomposition, simply specify
:raw-latex:`\fBdecomposition`=:raw-latex:`\fIDECOMP`\_CONFIG_JSON_FILE:raw-latex:`\fR `option
in the :raw-latex:`\fBstrgp`\_add:raw-latex:`\fR` command. There are
three types of decompositions: :raw-latex:`\fBstatic`:raw-latex:`\fR`,
:raw-latex:`\fBas`\_is:raw-latex:`\fR`, and ``flex``.
:raw-latex:`\fBstatic`:raw-latex:`\fR `decomposition statically and
strictly decompose LDMS set according to the definitions in the
:raw-latex:`\fIDECOMP`\_CONFIG_JSON_FILE:raw-latex:`\fR`.
:raw-latex:`\fBas`\_is:raw-latex:`\fR `decomposition on the other hand
takes all metrics and converts them as-is into rows.
:raw-latex:`\fBflex`:raw-latex:`\fR `decomposition applies various
decompositions by LDMS schema digest mapping from the configuration.

Please see section :raw-latex:`\fBSTATIC `DECOMPOSITION:raw-latex:`\fR`,
:raw-latex:`\fBAS`\_IS DECOMPOSITION:raw-latex:`\fR `, and
:raw-latex:`\fBFLEX `DECOMPOSITION:raw-latex:`\fR `for more information.

More decomposition types may be added in the future. The decomposition
mechanism is pluggable. Please see
:raw-latex:`\fBas`\_is:raw-latex:`\fR`,
:raw-latex:`\fBstatic`:raw-latex:`\fR`, and
:raw-latex:`\fBflex`:raw-latex:`\fR `decomposition implementation in
:``ldms/src/decomp/`` directory in the source tree for more information.

.SH STATIC DECOMPOSITION The
:raw-latex:`\fBstatic`:raw-latex:`\fR `decomposition statically and
strictly converts LDMS set to one or more rows according to the
:raw-latex:`\fIDECOMP`\_CONFIG_JSON_FILE:raw-latex:`\fR`. The format of
the JSON configuration file is as follows:

.EX { “type”: :raw-latex:`\fB`“static”:raw-latex:`\fR`, “rows”: [ {
“schema”: ":raw-latex:`\fIOUTPUT`\_ROW_SCHEMA:raw-latex:`\fR`“,”cols“: [
{ "src":":raw-latex:`\fILDMS`\_METRIC_NAME:raw-latex:`\fR`",
"dst":":raw-latex:`\fIDST`\_COL_NAME:raw-latex:`\fR`",
"type":":raw-latex:`\fITYPE`:raw-latex:`\fR`", "rec_member":
":raw-latex:`\fIREC`\_MEMBER_NAME_IF_SRC_IS_RECORD:raw-latex:`\fR`",
"fill": :raw-latex:`\fIFILL`\_VALUE:raw-latex:`\fR`, "op":
":raw-latex:`\fIOPERATION`:raw-latex:`\fR`" }, … ],”indices“: [ {
"name": ":raw-latex:`\fIINDEX`\_NAME:raw-latex:`\fR`", "cols": [
":raw-latex:`\fIDST`\_COL_NAME:raw-latex:`\fR`", … ] }, … ],”group“:
{”limit": :raw-latex:`\fIROW`\_LIMIT:raw-latex:`\fR`, “index”: [
":raw-latex:`\fIDST`\_COL_NAME:raw-latex:`\fR`", … ], “order”: [
":raw-latex:`\fIDST`\_COL_NAME:raw-latex:`\fR`", … ], “timeout”:
“:raw-latex:`\fITIME`:raw-latex:`\fR`” } }, … ] } .EE

The :raw-latex:`\fB`“rows”:raw-latex:`\fR `is an array of row definition
object, each of which defines an output row. Each row definition
contains: .RS 2 - :raw-latex:`\fB`“schema”:raw-latex:`\fR`: a string
specifying output schema name, .br -
:raw-latex:`\fB`“cols”:raw-latex:`\fR`: a list of column definitions,
.br - :raw-latex:`\fB`“indices”:raw-latex:`\fR`: an optional list of
index definitions for the storage technologies that require or support
indexing, and .br - :raw-latex:`\fB`“group”:raw-latex:`\fR`: a grouping
definition for “op” operations (“group” is not required if “op” is not
specified; see “op” and “group” below). .RE

The detail explanation of :raw-latex:`\fB`“cols”:raw-latex:`\fR`,
:raw-latex:`\fB`“indices”:raw-latex:`\fR `and
:raw-latex:`\fB`“group”:raw-latex:`\fR` are as follows.

.TP 1 :raw-latex:`\fB`“cols”:raw-latex:`\fR` Each column object in
:raw-latex:`\fB`“cols”:raw-latex:`\fR `contains the following
attributes:

.RS 4

.TP 4 :raw-latex:`\fB`“src”:raw-latex:`\fR `:
":raw-latex:`\fILDMS`\_METRIC_NAME:raw-latex:`\fR`" This refers to the
metric name in the LDMS set to be used as the source of the
decomposition. :raw-latex:`\fILDMS`\_METRIC_NAME:raw-latex:`\fR `can
also be specified in the form of
“:raw-latex:`\fILIST`:raw-latex:`\fR[\fIMEMBER\fR]`” to refer to MEMBER
of the record in the list NAME. For example,

.EX “src” : “netdev_list[rx_bytes]” .EE

refers to the “rx_bytes” member of records in “netdev_list”.

The :raw-latex:`\fB`“timestamp”:raw-latex:`\fR`,
:raw-latex:`\fB`“producer”:raw-latex:`\fR`, and
:raw-latex:`\fB`“instance”:raw-latex:`\fR `are special “src” that refer
to update timestamp, producer name and instance name of the set
respectively.

.TP :raw-latex:`\fB`“dst”:raw-latex:`\fR `:
":raw-latex:`\fIDST`\_COL_NAME:raw-latex:`\fR`" (optional) This is the
name of the output column, later consumed by storage policy. If not
specified, the
:raw-latex:`\fILDMS`\_METRIC_NAME:raw-latex:`\fR `specified in “src” is
used.

.TP :raw-latex:`\fB`“type”:raw-latex:`\fR `:
“:raw-latex:`\fITYPE`:raw-latex:`\fR`” (required if “fill” is specified)
The type of the output column. This is required if “fill” attribute if
specified. If “fill” is not specified, “type” is optional. In such case,
the type is the first discovery from the metric value in the LDMS set
processed by this decomposition.

.TP :raw-latex:`\fB`“rec_member”:raw-latex:`\fR `:
":raw-latex:`\fIMEMBER`\_NAME:raw-latex:`\fR`" (optional) If “src”
refers to a list of records or an array of records, “rec_member” can be
specified to access the member of the records. Alternatively, you can
use “:raw-latex:`\fILIST`:raw-latex:`\fR[\fIMEMBER\fR]`” form in “src”
to access the member in the records.

.TP :raw-latex:`\fB`“fill”:raw-latex:`\fR `:
:raw-latex:`\fIFILL`\_VALUE:raw-latex:`\fR `(optional) This is the value
used to fill in place of “src” in the case that the LDMS set does not
contain “src” metric. The
:raw-latex:`\fIFILL`\_VALUE:raw-latex:`\fR `can also be an array. If
“src” is not found in the LDMS set and “fill” is not specified, the LDMS
set is skipped.

.TP :raw-latex:`\fB`“op”:raw-latex:`\fR `:
“:raw-latex:`\fIOPERATION`:raw-latex:`\fR`” (optional) If “op” is set,
the decomposition performs the specified
:raw-latex:`\fIOPERATION`:raw-latex:`\fR `on the column.
:raw-latex:`\fB`“group”:raw-latex:`\fR `must be specified in the
presence of “op” so that the decomposition knows how to group previously
produced rows and perform the operation on the column of those rows.
Please see :raw-latex:`\fB`“group”:raw-latex:`\fR `explanation below.

The supported :raw-latex:`\fIOPERATION`:raw-latex:`\fR `are “diff”,
“min”, “max”, and “mean”.

.RE

.TP 1 :raw-latex:`\fB`“indices”:raw-latex:`\fR` The “indices” is a list
of index definition objects. Each index definition object contains
:raw-latex:`\fB`“name”:raw-latex:`\fR `(the name of the index) and
:raw-latex:`\fB`“cols”:raw-latex:`\fR `which is the names of the OUTPUT
columns comprising the index.

.TP 1 :raw-latex:`\fB`“group”:raw-latex:`\fR` The
:raw-latex:`\fB`“group”:raw-latex:`\fR `is an object defining how
:raw-latex:`\fB`“op”:raw-latex:`\fR `identify rows to operate on. The
:raw-latex:`\fBREQUIRED`:raw-latex:`\fR `attributes and their
descriptions for the :raw-latex:`\fB`“group”:raw-latex:`\fR` object are
as follows:

.RS 4

.TP 4 :raw-latex:`\fB`“index”:raw-latex:`\fR `: [
":raw-latex:`\fIDST`\_COL_NAME:raw-latex:`\fR`", … ] This is a list of
columns that defines the grouping index. If two rows r0 and r1 have the
same value in each of the corresponding columns, i.e. for k in index:
r0[k] == r1[k], the rows r0 and r1 belong to the same group.

.TP 4 :raw-latex:`\fB`“order”:raw-latex:`\fR `: [
":raw-latex:`\fIDST`\_COL_NAME:raw-latex:`\fR`", … ] This is a list of
columns used for orering rows in each group (in descending order). For
example, ``[ "timestamp" ]`` orders each group (in descending order)
using “timestamp” column.

.TP 4 :raw-latex:`\fB`“limit”:raw-latex:`\fR `:
:raw-latex:`\fIROW`\_LIMIT:raw-latex:`\fR` This is an integer limiting
the maximum number of rows to be cached in each group. The first
:raw-latex:`\fIROW`\_LIMIT:raw-latex:`\fR `rows in the group
descendingly ordered by :raw-latex:`\fB`“order”:raw-latex:`\fR `are
cached. The rest are discarded.

.TP 4 :raw-latex:`\fB`“timeout”:raw-latex:`\fR `:
“:raw-latex:`\fITIME`:raw-latex:`\fR`” The amount of time (e.g. “30m”)
of group inactivity (no row added to the group) to trigger row cache
cleanup for the group. If this value is not set, the row cache won’t be
cleaned up. .RE

.TP 1 .B Static Decomposition Example 1: simple meminfo with fill The
following is an example of a static decomposition definition converting
meminfo set into two schemas, “meminfo_filter” (select a few metrics)
and “meminfo_directmap” (select a few direct map metrics with “fill”
since DirectMap varies by CPU architecture).

.EX { “type”: “static”, “rows”: [ { “schema”: “meminfo_filter”, “cols”:
[ { “src”:“timestamp”, “dst”:“ts” }, { “src”:“producer”, “dst”:“prdcr”
}, { “src”:“instance”, “dst”:“inst” }, { “src”:“component_id”,
“dst”:“comp_id” }, { “src”:“MemFree”, “dst”:“free” }, {
“src”:“MemActive”, “dst”:“active” } ], “indices”: [ {
“name”:“time_comp”, “cols”:[“ts”, “comp_id”] }, { “name”:“time”,
“cols”:[“ts”] } ] }, { “schema”: “meminfo_directmap”, “cols”: [ {
“src”:“timestamp”, “dst”:“ts” }, { “src”:“component_id”, “dst”:“comp_id”
}, { “src”:“DirectMap4k”, “dst”:“directmap4k”, “type”:“u64”, “fill”: 0
}, { “src”:“DirectMap2M”, “dst”:“directmap2M”, “type”:“u64”, “fill”: 0
}, { “src”:“DirectMap4M”, “dst”:“directmap4M”, “type”:“u64”, “fill”: 0
}, { “src”:“DirectMap1G”, “dst”:“directmap1G”, “type”:“u64”, “fill”: 0 }
], “indices”: [ { “name”:“time_comp”, “cols”:[“ts”, “comp_id”] }, {
“name”:“time”, “cols”:[“ts”] } ] } ] } .EE

.TP 1 .B Static Decomposition Example 2: record with “op” The following
is an example of a static decomposition with “rec_member” usage in
various forms and with “op”.

.EX { “type”: “static”, “rows”: [ { “schema”: “netdev2_small”, “cols”: [
{ “src”:“timestamp”, “dst”:“ts”, “type”:“ts” }, { “src”:“producer”,
“dst”:“prdcr”, “type”:“char_array” }, { “src”:“instance”, “dst”:“inst”,
“type”:“char_array” }, { “src”:“component_id”, “dst”:“comp_id”,
“type”:“u64” }, { “src”:“netdev_list”, “rec_member”:“name”,
“dst”:“netdev.name” }, { “src”:“netdev_list[rx_bytes]”,
“dst”:“netdev.rx_bytes” }, { “src”:“netdev_list[tx_bytes]” }, {
“src”:“netdev_list[rx_bytes]”, “op”: “diff”,
“dst”:“netdev.rx_bytes_diff” }, { “src”:“netdev_list[tx_bytes]”, “op”:
“diff”, “dst”:“netdev.tx_bytes_diff” } ], “indices”: [ {
“name”:“time_comp”, “cols”:[“ts”, “comp_id”] }, { “name”:“time”,
“cols”:[“ts”] } ], “group”: [ “limit”: 2, “index”: [ “comp_id”,
“netdev.name” ], “order”: [ “ts” ], “timeout”: “60s” ] } ] } .EE

The “name” record member will produce “netdev.name” column name and
“rx_bytes” record member will produce “netdev.rx_bytes” column name as
instructed, while “tx_bytes” will produce “netdev_list[tx_bytes]” column
name since its “dst” is omitted.

The “netdev.rx_bytes_diff” destination column has “op”:“diff” that
calculate the difference value from “src”:“netdev_list[rx_bytes]”. The
“group” instructs “op” to group rows by [“comp_id”, “netdev.name”],
i.e. the “diff” will be among the same net device of the same node
(comp_id). The “order”:[“ts”] orders the rows in the group by “ts” (the
timestamp). The “limit”:2 keeps only 2 rows in the group (current and
previous row by timestamp). The “timeout”: “60s” indicates that if a
group does not receive any data in 60 seconds (e.g. by removing a
virtual network device), the row cache for the group will be cleaned up.

The “netdev.tx_bytes_diff” is the same as “netdev.rx_bytes_diff” but for
tx_bytes.

Assuming that the “netdev_list” has N records in the list, the
decomposition will expand the set into N rows.

.SH AS_IS DECOMPOSITION The
:raw-latex:`\fBas`\_is:raw-latex:`\fR `decomposition generate rows as-is
according to metrics in the LDMS set. To avoid schema conflict, such as
meminfo collecting from heterogeneous CPU architectures,
:raw-latex:`\fBas`\_is:raw-latex:`\fR `decomposition appends the short
LDMS schema digest (7 characters) to the row schema name before
submitting the rows to the storage plugin. For example, “meminfo” LDMS
schema may turn into “meminfo_8d2b8bd” row schema. The
:raw-latex:`\fBas`\_is:raw-latex:`\fR `decomposition configuration only
takes “indices” attribute which defines indices for the output rows.
When encountering a list of primitives, the as_is decomposition expands
the set into multiple rows (the non-list metrics’ values are repeated).
When encountering a list of records, in addition to expanding rows, the
decomposition also expand the record into multiple columns with the name
formatted as “LIST_NAME.REC_MEMBER_NAME”. The “timestamp” is not a
metric in the set but it is used in all storage plugins. So, the
“timestamp” column is prepended to each of the output rows.

The format of the JSON configuration is as follows:

.EX { “type”: “as_is”, “indices”: [ { “name”: “INDEX_NAME”, “cols”: [
COLUMN_NAMES, … ] }, … ] } .EE

The following is an :raw-latex:`\fBas`\_is:raw-latex:`\fR `decomposition
configuration example with two indices:

.EX { “type”: “as_is”, “indices”: [ { “name”: “time”, “cols”: [
“timestamp” ] }, { “name”: “time_comp”, “cols”: [ “timestamp”,
“component_id” ] } ] } .EE

.SH FLEX DECOMPOSITION The
:raw-latex:`\fBflex`:raw-latex:`\fR `decomposition applies various
decompositions by LDMS schema digests specified in the configuration.
The configurations of the applied decompositions are also specified in
``flex`` decomposition file as follows:

.EX { “type”: “flex”, /\* defining decompositions to be applied */
“decomposition”: { “”: { “type”: “”, … }, … }, /* specifying digests and
the decompositions to apply */ “digest”: { “”: “”, “”: [ “”, “” ], …
"*\ “:”" /\* optional : the unmatched \*/ } } .EE

.B Example: In the following example, the “meminfo” LDMS sets have 2
digests due to different metrics from different architecture. The
configuration then maps those digests to “meminfo” static decomposition
(producing “meminfo_filter” rows). It also showcases the ability to
apply multiple decompositions to a matching digest. The procnetdev2 sets
with digest
“E8B9CC8D83FB4E5B779071E801CA351B69DCB9E9CE2601A0B127A2977F11C62A” will
have “netdev2” static decomposition and “the_default” as-is
decomposition applied to them. The sets that do not match any specific
digest will match the "*" digest. In this example, “the_default” as-is
decomposition is applied.

.EX { “type”: “flex”, “decomposition”: { “meminfo”: { “type”: “static”,
“rows”: [ { “schema”: “meminfo_filter”, “cols”: [ { “src”:“timestamp”,
“dst”:“ts”, “type”:“ts” }, { “src”:“producer”, “dst”:“prdcr”,
“type”:“char_array”, “array_len”:64 }, { “src”:“instance”, “dst”:“inst”,
“type”:“char_array”, “array_len”:64 }, { “src”:“component_id”,
“dst”:“comp_id”, “type”:“u64” }, { “src”:“MemFree”, “dst”:“free”,
“type”:“u64” }, { “src”:“MemActive”, “dst”:“active”, “type”:“u64” } ],
“indices”: [ { “name”:“time_comp”, “cols”:[“ts”, “comp_id”] }, {
“name”:“time”, “cols”:[“ts”] } ] } ] }, “netdev2” : { “type” : “static”,
“rows”: [ { “schema”: “procnetdev2”, “cols”: [ { “src”:“timestamp”,
“dst”:“ts”,“type”:“ts” }, { “src”:“component_id”,
“dst”:“comp_id”,“type”:“u64” }, { “src”:“netdev_list”,
“rec_member”:“name”, “dst”:“dev.name”, “type”:“char_array”, “array_len”:
16 }, { “src”:“netdev_list”, “rec_member”:“rx_bytes”,
“dst”:“dev.rx_bytes”, “type”:“u64” }, { “src”:“netdev_list”,
“rec_member”:“tx_bytes”, “dst”:“dev.tx_bytes”, “type”:“u64” } ],
“indices”: [ { “name”:“time_comp”, “cols”:[“ts”, “comp_id”] } ] } ] },
“the_default”: { “type”: “as_is”, “indices”: [ { “name”: “time”, “cols”:
[ “timestamp” ] }, { “name”: “time_comp”, “cols”: [ “timestamp”,
“component_id” ] } ] } }, “digest”: {
“71B03E47E7C9033E359DB5225BC6314A589D8772F4BC0866B6E79A698C8799C0”:
“meminfo”,
“59DD05D768CFF8F175496848486275822A6A9795286FD9B534FDB9434EAF4D50”:
“meminfo”,
“E8B9CC8D83FB4E5B779071E801CA351B69DCB9E9CE2601A0B127A2977F11C62A”: [
“netdev2”, “the_default” ], "\*“:”the_default" } } .EE

.SH SEE ALSO Plugin_store_sos(7), Plugin_store_csv(7),
Plugin_store_kafka(7)
