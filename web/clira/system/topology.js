/*
 * $Id$
 *  -*-  indent-tabs-mode:nil -*-
 * Copyright 2013, Juniper Network Inc.
 * All rights reserved.
 * This SOFTWARE is licensed under the LICENSE provided in the
 * ../Copyright file. By downloading, installing, copying, or otherwise
 * using the SOFTWARE, you agree to be bound by the terms of that
 * LICENSE.
 */

jQuery(function ($) {
    jQuery.clira.commandFile({
        name: "topology",
        prereqs: [
            "/external/d3/d3.v3.js",
        ],
        commands: [
            {
                command: "show ospf topology",
                help: "Demo: ospf topology in a force graph",
                arguments: [
                    {
                        name: "data",
                        type: "string",
                    },
                ],
                execute: topology,
            }
        ],
    });

    var image = {
        unknown: "/images/icons/dot-dot.jpg",
        network: "/images/icons/100baset_hub.jpg",
        stub: "/images/icons/cloud.jpg",
        transit: "/images/icons/arrows_motion.jpg",
        router: "/images/icons/router.jpg",
    }
    var colors = { };

    function topology ($output, cmd, parse, poss) {
        var color_wheel = d3.scale.category20();
        var width = 960;
        var height = 500;

        var key, count = 0;
        if (0) {
            for (key in image) {
                colors[key] = color_wheel[count++];
            }
        }

        var force = d3.layout.force()
            .charge(function (node) {
                if (node.different)
                    return -800;
                return (node.type == "router") ? -1000 : -400;
            })
            .linkDistance(function (link) {
                /* XXX Turns out different isn't that special */
                return link.different ? 120 : 120;
            })
            .size([width, height]);

        var svg = d3.select($output.get(0)).append("svg")
            .attr("width", width)
            .attr("height", height);

        d3.json("/bin/topology-ospf.slax", function(error, graph) {
            if (error) {
                $.clira.makeAlert($output, error);
                return;
            }

            if (graph.error) {
                $.clira.makeAlert($output, graph.error);
                return;
            }

            if (!(graph.nodes && graph.links)) {
                $.clira.makeAlert($output, "invalid topology data");
                return;
            }

            force
                .nodes(graph.nodes)
                .links(graph.links)
                .start();

            var link = svg.selectAll(".link")
                .data(graph.links)
                .enter().append("line")
                .attr("class", "link")
                .style("stroke-width", function(d) {
                return Math.sqrt(d.value);
            });

            var node = svg.selectAll(".node")
                .data(graph.nodes)
                .enter()
                .append("g")
                .attr("class", function (node) {
                    return "node node-" + node.type;
                })
/*
                .attr("r", function (node) {
                    return (node.type == "router") ? 15: 8;
                })
                .style("fill", function(node) {
                    return colors[node.type];
                })
*/
                .call(force.drag);

/*
            var circles = node.append("circle")
                .attr("r", function (node) {
                    return (node.type == "router") ? 15: 8;
                })
                .style("fill", function(d) {
                    return color_wheel(d.group);
                });
*/

            node.append("text")
                .attr("dx", -48)
                .attr("dy", 30)
                .text(function (node) { return node.name });

            node.append("image")
                .attr("x", -24)
                .attr("y", -24)
                .attr("width", 48)
                .attr("height", 48)
                .attr("xlink:href", function (node) {
                    return image[node.type] ? image[node.type] : image.unknown;
                });
               

            node.append("title")
                .text(function(node) {
                    return node.type + ": " + node.name;
                });

/*
            node.append("image")
                .attr("xlink:href", function (node) {
                    return image[node.type] ? image[node.type] : image.unknown;
                })
                .attr("x", -12)
                .attr("y", -12)
                .attr("width", 24)
                .attr("height", 24);

            node.append("text")
                .attr("dx", 12)
                .attr("dy", ".35em")
                .text(function (node) { return node.name });
*/

            force.on("tick", function() {
                link.attr("x1", function(d) { return d.source.x; })
                    .attr("y1", function(d) { return d.source.y; })
                    .attr("x2", function(d) { return d.target.x; })
                    .attr("y2", function(d) { return d.target.y; });

                node.attr("transform", function (node) {
                    return "translate(" + node.x + "," + node.y + ")";
                });
/*
                node.attr("cx", function(d) { return d.x; })
                    .attr("cy", function(d) { return d.y; });
*/
            });
        });
    }
});

