<!-- Use "pandoc -sS -o README.html README.md" to process this to HTML -->

# URBAN HEART BEAT PROJECT

This directory contains applications (and whatever) to support the
Urban Heart Beat project from TerraSwarm.

This is included in the default git tree, but it isn't a make target
from the root directory because it requires some other libraries that
aren't all that common (notably mosquitto).

## Required Packages

The mqtt-gdp-gateway application requires Mosquitto, an MQTT
client library.  On Debian and Ubuntu you'll need to install
`libmosquitto0-dev`.  On FreeBSD the package is named `mosquitto`.
On MacOS mosquitto is supported by brew, but not bsdports.
However it is easy to download and compile the library from source.


## What's in This Directory?

### mqtt-gdp-gateway

The mqtt-gdp-gateway application that reads one or more MQTT topics
(specified by a pattern) and copies all messages it receives to a GDP
log.

For the moment mqtt-gdp-gateway assumes that all MQTT messages are
formatted as JSON, and (assuming this is true) writes records that
are formatted as JSON.  The output format is "wrapped" with a JSON
object to include metadata.

MQTT topics are specified using a standard topic pattern.  Quoting
from the mosquitto mqtt(7) man page:

---

Topics are treated as a hierarchy, using a slash (/) as a separator.
This allows sensible arrangement of common themes to be created,
much in the same way as a filesystem. For example, multiple
computers may all publish their hard drive temperature information
on the following topic, with their own computer and hard drive name
being replaced as appropriate:

    sensors/COMPUTER_NAME/temperature/HARDDRIVE_NAME

Clients can receive messages by creating subscriptions. A subscription
may be to an explicit topic, in which case only messages to that topic
will be received, or it may include wildcards. Two wildcards are
available, `+` or `#`.

`+` can be used as a wildcard for a single level of hierarchy. It could
be used with the topic above to get information on all computers and
hard drives as follows:

    sensors/+/temperature/+

As another example, for a topic of "a/b/c/d", the following example
subscriptions will match:

    * a/b/c/d
    * +/b/c/d
    * a/+/c/d
    * a/+/+/d
    * +/+/+/+

The following subscriptions will not match:

    * a/b/c
    * b/+/c/d
    * +/+/+

`#` can be used as a wildcard for all remaining levels of
hierarchy. This means that it must be the final character in a
subscription. With a topic of "a/b/c/d", the following example
subscriptions will match:

    * a/b/c/d
    * #
    * a/#
    * a/b/#
    * a/b/c/#
    * +/b/c/#

Zero length topic levels are valid, which can lead to some slightly
non-obvious behaviour. For example, a topic of "a//topic" would
correctly match against a subscription of "a/+/topic". Likewise,
zero length topic levels can exist at both the beginning and the
end of a topic string, so "/a/topic" would match against a
subscription of "+/a/topic", "#" or "/#", and a topic "a/topic/"
would match against a subscription of "a/topic/+" or "a/topic/#".

---

See the mqtt-gdp-gateway(1) man page for more details.
