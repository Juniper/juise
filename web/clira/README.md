<!---
# Copyright 2014, Juniper Networks, Inc.
# All rights reserved.
# This SOFTWARE is licensed under the LICENSE provided in the
# Copyright file. By downloading, installing, copying, or otherwise
# using the SOFTWARE, you agree to be bound by the terms of that
# LICENSE.
#-->

Writing CLIRA apps
---
A typical __CLIRA__ app consists of a _command file_ which provides possible 
commands for the app that the user can run and a _handlebars template(s)_ 
file to format and display output to the user. Command files are located in 
`juise/web/clira/system` directory and templates are located in 
`juise/web/clira/templates/`. Learn more about handlebars templates at 
[http://handlebarsjs.com/](http://handlebarsjs.com/) and 
[http://emberjs.com/guides/templates/handlebars-basics/]
(http://emberjs.com/guides/templates/handlebars-basics/). Most of the basic 
views that are required for applications are available in Ember and some 
extended views specific to __CLIRA__ are available in 
`juise/web/clira/views.js`. __CLIRA__ is built using ember framework. Should 
you need any more views than already available, please read about  emberjs 
and extending views at [http://emberjs.com/guides/getting-started/]
(http://emberjs.com/guides/getting-started/).

###Adding command file
Loading a command file calls `clira.commandFile()` function with an array of 
commands to be loaded into the app.  `clira.commandFile()` function reads an 
object with following properties

* __name__: name for the command file / package
* __templatesFile__: optional path to file containing handlebars templates 
for commands. Note that templates file will have each template in <script> 
tag with template name available in data-template-name
* __commands__: Array containing command objects

####Each command object has following properties:

* __command__: Command that will be available to the user
* __help__: Help message briefly describing this command
* __arguments__: Array of argument objects defining the arguments available 
under this command
* __templateName__: When a corresponding templates file is available and 
specified using templatesFile is specified for this commands file, this tells 
the name of the template among the templates available in specified file to 
be used for this command
* __templateFile__: optional name of the template file for this command. If a 
file containing template for this command alone is being used, this option 
will be specified. Template file will simply contain contents of template 
without any script tag
* __execute__: Function that is executed when the user runs this command. It 
has following prototype
     * `function(view, cmd, parse, poss)`
         * _view_: Ember view corresponding to the output block for this command
         * _cmd_: Command that is being executed
         * _parse_: Parse object for the given command
         * _poss_: possibilities object for the given command
* __onOutputChange__: Optional function that gets called when there is a change 
in output and we have to deal with updating view. It has the same prototype as 
execute()

####_argument_ object has following properties:

* __name__: name/keyword for the argument
* __help__: help string for this argument
* __type__: type of this argument
* __nokeyword__: optional boolean that specifies if user has to provide or not
* __multiple_words__: optional boolean that tells that the value is multi-worded

###Sample command file

    jQuery(function($) {
        jQuery.clira.commandFile({
            name: "debug",
            commands: [{
                command: "enable debugging",
                help: "Log debug messages",
                execute: function (view, cmd, parse, poss) {
                    localStorage['debug'] = true;
                    view.get('controller').set('output', 'Enabled debugging');
                }
            }]
        });
    });


###Updating output view
Ember view corresponding to the output block is available to the execute 
function as view argument. If a template is being used, setting output 
variable on view’s controller would make the output data available in the 
template.  
  
    view.get('controller').set('output', output);

in execute function will make the data in output available in template 
associated with this view. If there is no template available for this command, 
output will need to be a string that will be rendered in the output container 
block.

###Loading command files
To load new command files, user will have to place his command file in 
`juise/web/clira/system`, template(s) file in `juise/web/clira/templates` and 
run `reload commands` in __CLIRA__ which loads all the commands from all the 
files available under `system/`.

###Communicating with device
Most of the commands will need to connect to a device, execute RPCs and use 
the result set. __CLIRA__ provides two ways to retrieve data from device

* __$.clira.runSlax()__: One can use this function to run _SLAX_ scripts where 
user can connect to a device, run RPCs and extract required data. It takes a 
JSON object with following properties as argument
    * _script_: Path to script location
    * _args_: Object containing the variables as keys and data as values which 
    will be available to the script
    * _view_: View to be used to render the data into
    * _success_: Function that will be called when script is run successfully
    * _failure_: Callback function in the event of script failure

 For example, you can use this function as below    

            $.clira.runSlax({
                script: '/clira/system/topology.slax',
                args: {
                    target: poss.data.target
                },
                view: view,
                success: function (data) {
                    renderTopology(view, data);
                },
                failure: function (data) {
                    $.clira.makeAlert(view, "Error executing command: " + data);
                }
            });

* __$.clira.runCommand()__: This function can be used to execute a command on 
target device and render output in the output container. This function takes 
the following arguments
    * _view_: View to render the output in
    * _target_: Target device to run the command on
    * _command_: Command to be executed

###Tracing
One can use browser’s __console__ functions to _trace/debug_ that application 
or can use `$.dbgpr` to log any messages that will be available in __CLIRA__ 
debug messages window using `show debug messages` command.

###Debugging
__CLIRA__ apps can be debugging just like a normal web application. You can 
use the tools available in the browser like [Chrome's developer tools panel]
(https://developer.chrome.com/devtools/index) or use [Firebug]
(https://getfirebug.com/) on Firefox. For inspecting ember aspect of the app, 
one can use [ember inspector]
(https://chrome.google.com/webstore/detail/ember-inspector/bmdblncegkenkacieihfhpjfppoconhi?hl=en) 
available for Chrome browser.
