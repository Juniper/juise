.. #
   #  -*-  indent-tabs-mode:nil -*-
   #
   # Copyright 2011-2024, Juniper Networks, Inc.
   # All rights reserved.
   # This SOFTWARE is licensed under the LICENSE provided in the
   # ../Copyright file. By downloading, installing, copying, or otherwise
   # using the SOFTWARE, you agree to be bound by the terms of that
   # LICENSE.
   #

.. default-role:: code

The JUISE Project
=================

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

JUISE scripts are typically written in SLAX, an alternative syntax for
XSLT.  For information about SLAX, refer to github.com/Juniper/libslax.
Additional reference material is available under the "JUNOS
Automation" at http://juniper.net.

The JUISE software can be used to execute scripts independent of a
JUNOS box, allowing the use of SLAX scripts in the off-box world.
Scripts can communicate with zero or more devices, allowing a
centralized manipulation of multiple devices.

JUISE includes a rudimentary NETCONF server, allowing JUNOS devices to
invoke RPCs to unix devices.  JUNOS devices can perform NETCONF or
JUNOScript RPCs to a JUISE box, and invoke scripts or programs on that
device. 

JUISE includes "mod_juise", a plug-in module for lighttpd, allowing
the execution of scripts via HTTP.  At this point, the portion of
JUISE is completely experimental.

Getting JUISE
-------------

JUISE is available as an open-source project via google code using the
following URL:

    https://github.com/Juniper/juise

Refer to the ./README file for building instructions.

Note that juise has a number of prerequisite software packages,
including libxslt, libxml2, libpcre, libslax, and others.

The "juise" Command
===================

The "juise" command is the central program of the JUISE project.  It
is used to test, format, convert, debug, and execute scripts.

The "juise" command has four distinct usage patterns, which are
discussed in this section.

- op script mode
- commit script mode
- server mode
- cgi mode

.. _syntax:

Command Syntax
--------------

The `juise` command line interface is as follows::

  Usage: juise [[user]@target] [options] [script] [param value]*

The `juise` command is normally invoked with the name of the script to
be run, a set of options are used to tailor the execution of the
script, and a set of name/value parameter pairs that are passed as
parameters to the script, as they are for the JUNOS "op" command.

The Target Argument
-------------------

If the first non-option argument contains an `'@'` and the `--target`
option is not present, the argument will be taken as the default
target.  This value will be used as the "local" device for
configuration retrieval and for any `jcs:open()` call that does not
provide an explicit target.  This argument can be either "user@device"
or "@device" (with the current login name used as the user name).

::

    juise my-login@my-router ...
    juise @my-router ...

Commit Scripts
==============

This section describes how commit scripts can be used inside juise. 

.. _history:

Overview (Function and History)
-------------------------------

Commit scripts were introduced in `JUNOS-7.2` as a means of adding
custom rules to the `commit check` process, allowing customers to
define additional validation rules which the incoming configuration
must satisfy.  Commit scripts can find and report configuration
statements that do not follow these rules.

Commit script can also change the configuration in two distinct ways.
Normal changes are added to the candidate configuration before it is
passed to the rest of the JUNOS subcomponents for validation.  These
changes are normal parts of the configuration data set.

In contrast, transient changes are not part of the normal
configuration data set, since they are not visible in the candidate
configuration nor are they saved in rollback data sets.  But they are
exposed to JUNOS subcomponents during validation, allowing the commit
script to create second order configuration information based on the
first order data in the candidate configuration.

The typical commit script relies heavily upon XPath to find errant
configuration.  They typically follow the pattern::

    if (xpath/to/find/bad/config) {
        report bad config;
    }

or::

    for-each (some/list/that/needs/to/be/checked) {
        if (xpath/to/find/bad/config) {
            report bad config;
        }
    }

Consider this example::

    version 1.1;

    import "../import/junos.xsl";

    match configuration {
        var $ldp = protocols/ldp;
        var $isis = protocols/isis;
        var $ospf = protocols/ospf;

        for-each ($isis/interface/name | $ospf/area/interface/name) {
            var $ifname = .;

            if (not($ldp/interface[name == $ifname])) {
                <xnm:error> {
                    call jcs:edit-path();
                    call jcs:statement();
                    <message> "ldp not enabled for this interface";
                }
            }
        }
    }

This script looks for interfaces listed under [protocols isis]
and [protocols ospf] to find interfaces not listed under [protocols
ldp].  When such interfaces are found, an error is generated.  The UI
sees this error and stops the configuration from being validated.

Testing Commit Scripts
----------------------

juise can be used to test commit scripts by using the `-c` option. The
target device can be given with either the `--target` (`-T`) option.  If
no target is given and the first argument contains an `@`, then this
is used as the target.

::

    % juise -c me@my-router my-script.slax

To execute the commit script, juise tries to reproduce the environment
which the script would run under JUNOS.  juise starts by downloading
the configuration in the same format used by on-box commit scripts,
with expanded inheritance and group and change attributes.  This
configuration is wrapped in an input document (the `<op-script-input>`
element) to be passed to the script.  The wrapper is identical to the
JUNOS wrapper, complete with the `<junos-context>` element.

The `<op-script-input>` element is passed as input to the script, which
runs normally, inspecting the configuration and generating,
`<xnm:error>`, `<xnm:warning>`, `<syslog>`, `<change>`, and
`<transient-change>` elements as needed.

The results of the script are inspected, and any <change> and
`<transient-change>` elements are uploaded to a `edit private` session.
Any errors are reported to the user.

The results of these interactions with the device are displayed in XML
to aid with debugging scripts.

If no errors have occurred, then any changes to the configuration can
be retrieved and reported to the user, who can verify that the changes
are those desired.  Use the "--output-format <style>" option to
retrieve the results in any of the following formats:

=========  ============================================
  Style     Description                                 
=========  ============================================
  compare   Display in "show compare" format (default)  
  none      Do not display changes                      
  text      Display in "show configuration" format      
  xml       Display in JUNOS XML format                
=========  ============================================

::

    % cat fix-domain.slax
    version 1.1;
    
    import "../import/junos.xsl";
    
    match configuration {
        <change> {
            <system> {
                <domain-name> "new.example.com";
            }
        }
    }
    % juise -c --output-format compare @dent fix-domain.slax
    
    Results from edit private:
    <xnm:warning>
    <message>
    uncommitted changes will be discarded on exit
    </message>
    </xnm:warning>
    
    Results from load change:
    <load-configuration-results>
    <load-success/>
    </load-configuration-results>
    
    Results from load change:
    <load-configuration-results>
    <load-success/>
    </load-configuration-results>
    
    Results from commit check:
    <commit-results>
    <load-success/>
    <routing-engine junos:style="normal">
    <name>re0</name>
    <commit-check-success/>
    </routing-engine>
    </commit-results>
    
    Results from script:
    <configuration-information>
    <configuration-output>
    [edit system]
    -  domain-name juniper.net;
    +  domain-name new.example.com;
    </configuration-output>
    </configuration-information>

Debugging Commit Scripts
------------------------

The libslax debugger (sdb) can be used to debug commit scripts.  Use
the "-d" option to trigger the debugger.

::

    % juise -d -c me@my-router my-script.slax
    sdb: The SLAX Debugger (version 0.12.2)
    Type 'help' for help
    (sdb) help
    List of commands:
      break [loc]     Add a breakpoint at [file:]line or template
      callflow [val]  Enable call flow tracing
      continue [loc]  Continue running the script
      delete [num]    Delete all (or one) breakpoints
      finish          Finish the current template
      help            Show this help message
      info            Showing info about the script being debugged
      list [loc]      List contents of the current script
      next            Execute the next instruction, stepping over
      over            Execute the current instruction hierarchy
      print <xpath>   Print the value of an XPath expression
      profile [val]   Turn profiler on or off
      reload          Reload the script contents
      run             Restart the script
      step            Execute the next instruction, stepping into
      verbose         Turn on verbose (-v) output logging
      where           Show the backtrace of template calls
      quit            Quit debugger

    Command name abbreviations are allowed
    (sdb) 

Op Scripts
==========

The second mode for juise is executing, testing, and debugging op
scripts.  This section discusses how to use juise for op scripts.

Overview (Function and History)
-------------------------------

`op` scripts were introduced in `JUNOS-7.5` as a means of adding custom
commands to the JUNOS CLI.  

Op scripts are not passed a normal document as input.  An
`<op-script-input>` element is passed in which contains the
`<junos-context>` similar to operation of op scripts in JUNOS.

Command line parameters (if any) are passed as SLAX parameters.  They
are also recorded under the <arguments> element inside the
`<junos-context>`.

Op scripts can generate output using the `message` statements and
`jcs:output` calls, as well as by the XML document which the script
generates.  The XML document is displayed only when script execution
is complete.

Running Op Scripts
------------------

The juise command can be used to execute op scripts::

    juise [options] [[user]@target] script-name [name value]*

The arguments to an op script are a name and a value, similar to the
"op" command under JUNOS::

    op script-name [name value]*

The name is arbitrary but should correspond to global parameters
declared inside the script using the "param" statement.

    juise @my-box my-script address 10.1.2.3 vlan fluffy

The script is run on the local machine, with jcs:open() able to
connect to the device, and output is displayed on the user's
terminal.

To invoke the debugger on a script, use the `-d` option::

    juise -d @my-box my-script address 10.1.2.3 vlan fluffy

Running Native Scripts
----------------------

juise can also be used to develop scripts that are not intended to run
under JUNOS.  The off-box environment gives a number of distinct
advantages for scripts that operate like `op` scripts.  These scripts
join the simplicity and flexibility of SLAX to the unix environment.

"#!/usr/local/bin/slax"
-----------------------

The SLAX parser supports the `"#!"` mechanism which is a normal part of
the unix environment.  This allows scripts beginning
with the line "#!/usr/bin/slax" to be executed directly from the
command line.

::

    #!/usr/local/bin/slax
    match / { message "this works"; }

If this script file is given the appropriate "executable" permission
bit (such as "chmod a+x works"), then the command "works" will work.

::

    % works
    this works
    %

Additional options can be added to the "#!" line::

    #!/usr/bin/slax -g --param check yes

Event Scripts
=============

Event scripts are not yet functional.  JUNOS currently lacks an
effective mechanism for getting event data from the device to a
transient listener like juise.

NETCONF Server
==============

juise can perform as a NETCONF server, accepting NETCONF RPCs and
returning XML data.  RPCs are serviced by invoking local scripts
on the server machine (aka `juise box`).

The server can be invoked in two ways.  The more traditional
invocation uses the `-R` or `--run-server` option.  The most common
way to run a NETCONF server is to make an :file:`sshd_config` file
listing the service.  In JUNOS, we do this as follows::

    Subsystem netconf /usr/libexec/ui/netconf

A juise box would require something like::

    Subsystem netconf /usr/local/bin/juise --run-server -P netconf

This service can be advertised using inetd with the following
configuration lines::

    netconf stream tcp nowait/75/150 root \
        /usr/sbin/sshd sshd -i -f /var/etc/sshd_config \
        -o SubsystemOnly=netconf -o Protocol=2
    netconf stream tcp6 nowait/75/150 root \
        /usr/sbin/sshd sshd -i -f /var/etc/sshd_config \
        -o SubsystemOnly=netconf -o Protocol=2

The lines are split here for readability but should be combined in
your inetd.conf file.

xml-mode
--------

The alternative means of running NETCONF is to use the `xml-mode`
command over a normal ssh connection.  This is non-standard, but
allows the deploying of NETCONF with no configuration and much less
impact.

juise will make a symbolic link from `juise` to `xml-mode`, so a
client can open a normal ssh command with the `xml-mode` command and
get a NETCONF connection.  We refer to this a "junos-netconf" and
the juise client supports it via the `-P` option and the
`junos-netconf` <protocol> option for `jcs:open()`.

RPCs
----

`juise` puts few limit limitations on the RPCs invoked on the server.
The RPC method name is used to locate a local script or binary which
implements that RPC.  The script or binary is executed with the RPC
content as standard input, and any output from the script is passed
back to the client.

The `JUISE_SCRIPT_DIR` environment variable and the `-D` option are
used to provide a set of search directories.  The default path is the
directory `$prefix/share/juise/scripts/`, or the value given
for `--with-script-dir=DIR` to the `configure` script at build time.

For each directory in the search path, juise looks for a file with the
name of the RPC method and a suitable extension.  The list of
extensions is "slax", "xsl", "xslt", "sh", "pl", and "" (none).

For example if a client sends an RPC like::

    <rpc>
        <get-some-information>
            <type>fish</type>
            <option>end-run</option>
        </get-some-information>
    </rpc>

The juise NETCONF server will look in for "get-some-information.slax"
in $prefix/share/juise/scripts/ and will load and run that script.

In the following example, the "test.slax" script is
accessed using the "test" RPC via the `xml-mode`
command.

::

    % cat /usr/local/share/juise/scripts/test.slax
    version 1.1;
    match / {
        <chassis-hardware> {
            <mumble> {
                <dink>;
            }
            <grumble> {
                <splat>;
            }
        }
    }
    % cat /tmp/foo.netconf
    <hello/>]]>]]>
    <rpc>
      <test/>
    </rpc>]]>]]>
    % ssh localhost xml-mode < /tmp/foo.netconf
    <?xml version="1.0"?>
    <hello xmlns="urn:ietf:params:xml:ns:netconf:base:1.0">
    <capabilities>
    <capability>urn:ietf:params:netconf:base:1.0</capability>
    </capabilities></hello>]]>]]>
    <rpc-reply>
    <?xml version="1.0"?>
    <chassis-hardware>
    <mumble><dink/></mumble><grumble><splat/></grumble>
    </chassis-hardware>
    </rpc-reply>]]>]]>
    %

The "juise" Command Reference
=============================

This section contains reference material for the juise command,
including command line options, environment variables, build options,
and other information.

"juise" Command Line Options
----------------------------

This section provides a complete listing of available juise options.

.. option:n: --agent
.. option:: -A

Enable forwarding of ssh-agent credentials, allowing remote ssh
sessions to connect to the local ssh-agent process for authentication
information.  See :manpage:`ssh-agent(1)` for details.

.. option:: --debug
.. option:: -d

Start `sdb`, the libslax debugger, allowing the user to
interactively debug script execution.  Refer to the debugger
documentation in the libslax distribution for more information
(github.com/Juniper/libslax).

= --directory <dir> OR -D <dir>
Use the given directory as the location for server scripts.  This
directory can also be set using the JUISE_SCRIPT_DIR environment
variable.

= --include <dir> OR -I <dir>
Add the given directory to the search path for files that are
referenced via the SLAX "include" or "import" statements.

= --indent OR -g
Make indented output as if the script contained::

    output-method {
        indent "yes";
    }

(The "-g" is for "good-looking output").

= --input <file> OR -i <file>
Use given file for input.

= --junoscript OR -J
Default to using the older JUNOScript XML API instead of the NETCONF
API for jcs:open() connections.

= --lib <dir> OR -L <dir>
Add the given directory to the list of directories searched for
dynamic extension libraries.  Refer to libslax for more information.

= --no-randomize
Avoid initializing the random number generator so script execution can
be predictable (e.g. during debugging).

= --param <name> <value> OR -a <name> <value>
An alternative method of giving parameters to a script.

= --protocol <name> OR -P <name>
Use the given protocol as the default protocol for jcs:open()
connections. 

= --run-server OR -R
Run juise in server mode, where it accepts incoming RPCs and executes
scripts given by the RPC name.  See ^server^ section for details.

= --script <name> OR -S <name>
An alternative method of giving the script name.

= --target <name> OR -T <name>
An alternative method of giving the default target name.

= --trace <file> OR -t <file>
Save trace data to the given file.

= --user <name> OR -u <name>
An alternative method of giving the user name for API connections.

= --verbose OR -v
Enable debugging output.  Any calls to slaxLog() will be displayed.

= --version OR -V
Display any version information, including the versions of libslax,
libxslt, and libxml2.  After displaying this information,
juise will exit.

= --wait <seconds>
After starting and parsing arguments, juise will wait for the
specified number of seconds, allowing the user to attach to the
process with "gdb" for additional debugging.

Environment Variables and configure.ac Settings
-----------------------------------------------

The values of certain environment variables and the command line
arguments to the "configure" script will affect the operation of
JUISE.  This section details these settings.

Setting The Import Path
~~~~~~~~~~~~~~~~~~~~~~~

JUNOS scripts typically import junos.xsl using the following
statement::

    import "../import/junos.xsl";

JUISE installs this file, along with other import files, in the
$prefix/share/juise/import directory.

In addition, JUISE will look at the environment variables JUISEPATH
and SLAXPATH.  These variables list a set of colon-separated
directories to be searched for import and include files.

Additional paths may be passed in on the command line using the "-I"
flag.

The `--with-juise-dir` argument of the configure script will also
affect this.  The contents of this option are appended with "/import"
and added to the initial search path.

Finding Scripts
~~~~~~~~~~~~~~~

If a valid filename is not passed to juise, it will look in
$prefix/share/juise/scripts/ for scripts.

mod_juise
---------

The JUISE project includes a plug-in module for the "lighttpd"
(pronounced "lightey") web server.  This plug-in allows scripts to be
run directly from lighttpd while allowing SSH credentials established
using "ssh-agent" to be inherited by the SLAX script.  SSH
connections, including those from the jcs:open() call, can then be
opened without prompting for a passphrase.

A suitable lighttpd.conf file is provided with juise, installed in the
$prefix/share/juise/web/ directory.  lighttpd can be started by using
the "-f" option and this file::

    lighttpd -f $prefix/share/juise/web/lighttpd.conf

To add mod_juise to an existing lighttpd server, add the following
line to your lighttpd.conf file::

    juise.assign = ( ".slax" => "juise.cgi" )

juise.cgi
~~~~~~~~~

When a file ending in ".slax" is requested, lighttpd will pass the
request to mod_juise which will invoke juise using the "juise.cgi"
driver.  This driver will fetch the CGI-defined variables and make
them available to the script as both global parameters and as elements
inside the "$cgi" global parameter.  Scripts are free to use either
method to retrieve these values.  The following table list the CGI
parameter name and the $cgi element name.

==================== ===================
  Parameter Name      Element Name       
==================== ===================
  CONTENT_LENGTH      content-length     
  DOCUMENT_ROOT       document-root      
  GATEWAY_INTERFACE   gateway-interface  
  HTTPS               https              
  LD_LIBRARY_PATH     ld-library-path    
  LD_PRELOAD          ld-preload         
  PATH_INFO           path-info          
  QUERY_STRING        query-string       
  REDIRECT_STATUS     redirect-status    
  REMOTE_ADDR         remote-addr        
  REMOTE_PORT         remote-port        
  REMOTE_USER         remote-user        
  REQUEST_METHOD      request-method     
  REQUEST_URI         request-uri        
  SCRIPT_FILENAME     script-filename    
  SCRIPT_NAME         script-name        
  SERVER_ADDR         server-addr        
  SERVER_NAME         server-name        
  SERVER_PORT         server-port        
  SERVER_PROTOCOL     server-protocol    
  SERVER_SOFTWARE     server-software    
  SYSTEMROOT          systemroot         
==================== ===================

The following lines are functionally equivalent:

    expr "User is " _ $REMOTE_USER;
    expr "User is " _ $cgi/remote-user;

In addition $cgi has an element named "parameters" which has the
fields of $QUERY_STRING broken into individual elements.  These are
also available as global parameters.  The following lines are
functionally equivalent: 

    expr "'from' parameter is " _ $cgi/parameters/from;
    expr "'from' parameter is " _ $from;

The full set of parameters can be accessed using this parameters
element::

    for-each ($cgi/parameters) {
        expr "Parameter '" _ name() _ "' is '" _ . _ "'";
    }

The <cgi> Element
~~~~~~~~~~~~~~~~~

When SLAX scripts are invoked via mod_juise, the script can choose to
emit a top level element named <cgi>.  This element is used to supply
directions to the HTTP server.

Attributes
~~~~~~~~~~

Any non-namespace attributes given on the <cgi> element is converted
into a header field in the HTTP reply message.  The attribute name is
the header field and the attribute value is the header value.

::

    <cgi Content-Type="text/html" X-Address=$REMOTE_ADDR>
        ...
    </cgi>

The above element would be turned into::

    Content-Type: text/html
    X-Address: 10.1.2.3

Additional attributes and elements may be defined under the <cgi>
element.  For future proofing, script should avoid attributes and
elements whose names being with "junos", "cgi", or "juise".

The MIXER Daemon
----------------

The MIXER daemon provides persistent SSH connections, allowing
multiple jcs:open calls to return more quickly by avoiding most of the
SSH connection establishment and negotiation expense.

MIXER can be used to forward normal SSH connections, but also gateways
WebSocket calls into NETCONF connections, allowing browser-based
software access to NETCONF services.

MIXER is currently under development.

Extension Libraries for juise
=============================

The JUISE project includes the jcs extension library which allows
invocation of NETCONF RPCs.  This library is identical to the jcs
library available under JUNOS.  This section details the extension
functions and element of those libraries.

{{note::
The jcs extension library is not a dynamic extension library and is
only available when using the juise command.  This needs to change,
but is low priority.
}}

The jcs Extensions
------------------

The "jcs" namespace is used for JUNOS-specific extension function, but
many of the functions are no longer JUNOS specific.  "jcs" was
originally for "JUNOS Commit Scripts", but these functions can be used
for other sorts of scripting as well.

In addition, many of the extension functions in the "slax" namespace
were originally in the "jcs" namespace, and are now available in both
namespaces.   The following table lists these dual-homed functions.

- break-lines    
- break_lines    
- dampen         
- empty          
- first-of       
- get-command    
- get-input      
- get-secret     
- getsecret      
- input          
- is-empty       
- output         
- progress       
- printf         
- regex          
- sleep          
- split
- sprintf        
- sysctl         
- syslog         
- trace          

jcs:open
~~~~~~~~

The jcs:open() function creates a connection to either the local
machine (for on-box JUNOS scripts) or a remote device using the
NETCONF or JUNOScript API.  This connection can be passed to
jcs:execute() to invoke RPCs and to jcs:close() to close the
connection.

Three protocols can be used for network access:
- netconf: IETF standard (RFC6241) over the standard netconf port with
the netconf subsystem
- junoscript: the original JUNOScript API accessed over the standard
ssh port using the JUNOS CLI
- junos-netconf: the NETCONF protocol accessed over the standard
ssh port using the JUNOS CLI

"junoscript" and "junos-netconf" have the advantage of not requiring
additional configuration, where "netconf" has the advantage of being
the full standard mechanism.

The behavior of jcs:open varies with the number of arguments given:

- If no arguments are passed and the script is running under JUNOS, a
  connection to the local device is created.  RPCs are processed within
  the device and no network login is needed.

- If no arguments are passed and the script is not running under
  JUNOS, the arguments to the juise program are using to build a
  connection.  The "--target", "--user", and "--protocol" options, along
  with the "[user]@target" argument (if provided), identify the target
  of the connection.

- If one argument is provided, the argument is the name of the device
  to connect with.  The name may be in either "target" or "user@target"
  format.

- If two arguments are provided, the first argument is the name of the
  device to connect with, and the second argument is a node-set with the
  following members.

===================== ======================================
  Element Name         Value                                  
===================== ======================================
  connection-timeout   Seconds before connect fails           
  method               netconf, junos-netconf, or junoscript  
  passphrase           SSH passphrase                         
  password             User password                          
  port                 Transport port number                  
  timeout              Connection timeout (in seconds)        
  username             User login name                        
===================== ======================================

- If three arguments are provided, the first argument is the name of
  the device to connect with, the second is the username, and the third
  is the password::

    SYNTAX::
        node-set jcs:open();
        node-set jcs:open(hostname);
        node-set jcs:open(hostname, info);
        node-set jcs:open(hostname, username, password);

    EXAMPLE::
        var $info = {
            <port> 5000;
            <timeout> 600;
        }
        var $conn = jcs:open($target, $info);

jcs:execute
~~~~~~~~~~~

The jcs:execute() function invokes RPCs using a connection returned by
jcs:open().  RPCs are invoked and their results returned using a
persistent connection, allowing stateful RPCs, such as locking the
configuration database.

The first argument is the connection, as returned by jcs:open().  If the
second argument is a string, it is the name of an RPC operation.
Otherwise the second argument is a node-set containing the operation
to be invoked.

::

    SYNTAX::
        node-set jcs:execute(conn, method);
        node-set jcs:execute(conn, rpc);

    EXAMPLE::
        var $rpc = <get-interface-information> {
            <interface-name> "fe-0/0/0";
            <statistics>;
        }
        var $res = jcs:execute($conn, $rpc);
        var $sw = jcs:execute($conn, "get-software-information");

jcs:invoke
~~~~~~~~~~

The jcs:invoke() function invokes RPCs but does not use or require a
persistent connection.  RPCs are stateless, since any connection
needed is temporary, so operations like locking the configuration
database should be avoided since the lock will be immediately
released.

If the argument to jcs:invoke is a string, it is the name of an RPC
operation.  Otherwise the argument is a node-set containing the
operation to be invoked.

::

    SYNTAX::
        node-set jcs:invoke(method);
        node-set jcs:invoke(rpc);

    EXAMPLE::
        var $rpc = <get-interface-information> {
            <interface-name> "fe-0/0/0";
            <statistics>;
        }
        var $res = jcs:invoke($rpc);
        var $sw = jcs:invoke("get-software-information");

jcs:close
~~~~~~~~~

The `jcs:close()` function closes a connection opened by jcs:open(),
releasing any resources on both the local and remote side.

    SYNTAX::
        void jcs:close(conn);
