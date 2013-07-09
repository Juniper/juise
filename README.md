<!---
# $Id$
#
# Copyright 2013, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# ../Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.
#-->

# JUISE: The JUNOS User Interface Scripting Environment

The JUISE project (the JUNOS User Interface Scripting Environment)
allows scripts to be written, debugged, and executed outside the
normal JUNOS environment.  Users can develop scripts in their desktop
environment, performing the code/test/debug cycle in a more natural
way.  Tools for developers are available, including a debugger, a
profiler, a call-flow tracing mechanism, and trace files.  
The JUNOS-specific extension functions are available for scripts,
allowing access to JUNOS XML data using the normal jcs:invoke and
jcs:execute functions.  Commit scripts can be tested against a JUNOS
device's configuration and the results of those script tested on that
device.

Check our
[github release page](https://github.com/Juniper/libslax/releases)
to find the latest release.

Please visit the 
[juise wiki](https://github.com/Juniper/juise/wiki)
for more information, documentation, examples, and notes on
JUISE and CLIRA.

<script type="text/javascript">

  var _gaq = _gaq || [];
  _gaq.push(['_setAccount', ' UA-25845345-1']);
  _gaq.push(['_trackPageview']);

  (function() {
    var ga = document.createElement('script'); ga.type = 'text/javascript'; ga.async = true;
    ga.src = ('https:' == document.location.protocol ? 'https://ssl' : 'http://www') + '.google-analytics.com/ga.js';
    var s = document.getElementsByTagName('script')[0]; s.parentNode.insertBefore(ga, s);
  })();

</script>
