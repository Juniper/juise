jQuery(function ($) {
    jQuery.clira.commandFile({
        name: "vcf",
        prereqs: [
            "/external/d3/d3.v3.js",
        ],
        commands: [{
            command: "show vcf topology",
            help: "Show Virtual Chassis Fabric topology",
            arguments: [{
                name: "target",
                type: "string",
                help: "Remote target to visualize",
                nokeyword: true,
                mandatory: true
            }],
            execute: vcfTopology
        }]
    });

    /*
     * Action function
     */
    function vcfTopology (view, cmd, parse, poss) {
        if (!poss.data.target) {
            $.clira.makeAlert(view, "You must include target to visualize");
            return;
        }

        $.clira.muxer();

        $.clira.runSlax({
            create: "yes",
            script: '/apps/vcf/vcf.slax',
            args: {
                target: poss.data.target
            },
            view: view,
            success: function (data) {
                renderVCFTopology(poss.data.target, view, data);
            },
            failure: function (data) {
                $.clira.makeAlert(view, "Error executing command: " + data);
            }
        });
    }

    /*
     * Renderer entry point
     */
    function renderVCFTopology(target, view, data) {
        if (!data)
            return;

        var width = 960,
            height = 360;

        var spineChildren = 0,
            leafChildren = 0,
            maxNodeWidth = 0,
            padding = 20,
            space = 5,
            blockHeight = 30;
        
        data = JSON.parse(data);

        var i;
        for (i = 0; i < data.spines.length; i++) {
            if (data.spines[i].children)
                spineChildren += data.spines[i].children.length;
        }
        for (i = 0; i < data.leaves.length; i++) {
            if (data.leaves[i].children)
                leafChildren += data.leaves[i].children.length;
        }

        if (spineChildren > leafChildren) {
            maxNodeWidth = (width - 2*padding - (spineChildren - 1)*space) / spineChildren;
        } else {
            maxNodeWidth = (width - 2*padding - (leafChildren - 1)*space) / leafChildren;
        }

        var controls = d3.select(view.$().get(0)).append("div");

        var showStats = false;

        /*
         * Append controls
         */
        controls.append("button")
                .text("Show Statistics")
                .on("click", function () {
                    if (showStats) {
                        hideStatistics();
                    } else {
                        displayStatistics();
                    }

                    showStats = ~showStats;
                    d3.select(this).text(function() {
                        return showStats ? "Hide Statistics" : "Show Statistics";
                    });
                })
                .attr("name", "stats");

        controls.append("span").text("  ");
        controls.append("label").text("Auto update").append("input")
                .attr("type", "checkbox")
                .attr("name", "autoupdate")
                .attr("value", false);

        controls.append("span").text("  ");
        controls.append("label").text("Update interval(in sec)").append("input")
                .attr("type", "text")
                .attr("name", "update-interval")
                .attr("size", 5)
                .attr("value", 10);

        /*
         * Handle auto update of stats
         */
        var updateInterval = null;
        view.$().find("input[name=autoupdate]").change(function() {
            if (this.checked) {
                if (showStats) {
                    updateInterval = setInterval(updateStatistics,
                        view.$().find("input[name=update-interval]").val() * 1000);
                }
            } else {
                if (updateInterval) clearInterval(updateInterval);
            }
        });

        var svg = d3.select(view.$().get(0)).append("svg")
                    .attr("width", width)
                    .attr("height", height);

        var spad = padding,
            lpad = padding;
        if (spineChildren > leafChildren) {
            lpad += (spineChildren - leafChildren)*(maxNodeWidth + space)/2;
        } else {
            spad += (leafChildren - spineChildren)*(maxNodeWidth + space)/2;
        }

        drawFPC(data.spines, spad, 30, true);
        drawFPC(data.leaves, lpad, 300, false);
        drawConnections(data.links);
        svg.append("text").text("SPINES").attr("x", width/2 - 20).attr("y", 20);
        svg.append("text").text("LEAVES").attr("x", width/2 - 20).attr("y", height - 10);

        /*
         * Removes ingress and egress bars from canvas
         */
        function hideStatistics() {
            d3.selectAll(".bars-ingress").remove();
            d3.selectAll(".bars-egress").remove();
            d3.selectAll(".ibarsbg").remove();
            d3.selectAll(".ebarsbg").remove();

            if (updateInterval) {
                clearInterval(updateInterval);
            }
        }

        /*
         * Overlays ingress and egress bars on canvas
         */
        function displayStatistics() {
            $.clira.runSlax({
                create: "yes",
                script: '/apps/vcf/vcf-stats.slax',
                args: {
                    target: target
                },
                view: view,
                success: function (data) {
                    drawBars(data);
                },
                failure: function (data) {
                    $.clira.makeAlert(view, "Error executing command: " + data);
                }
            });

            /*
             * If autoupdate is set, call to update statistics
             */
            if (view.$().find("input[name=autoupdate]").is(":checked")) {
                updateInterval = setInterval(updateStatistics,
                    view.$().find("input[name=update-interval]").val() * 1000);
            }
        }

        /*
         * Fetches and updates statistics on canvas
         */
        function updateStatistics() {
            $.clira.runSlax({
                create: "yes",
                script: '/apps/vcf/vcf-stats.slax',
                args: {
                    target: target
                },
                view: view,
                success: function (data) {
                    updateBars(data);
                },
                failure: function (data) {
                    $.clira.makeAlert(view, "Error executing command: " + data);
                }
            });
        }


        /*
         * Draws ingress and egress statistics on canvas
         */
        function drawBars(stats) {
            stats = JSON.parse(stats).stats;
            if (!stats)
                return;

            // Draw background for bars
            var ingressbg = svg.selectAll(".ibarsbg")
                    .data(stats)
                    .enter()
                    .append("rect")
                    .attr("x", function (d, i) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        return vcp.attr("x");
                    })
                    .attr("y", function (d, i) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        if (vcp.attr("y") < height/2)
                            return Number(vcp.attr("y")) + blockHeight;
                        else
                            return Number(vcp.attr("y")) - 60;
                    })
                    .attr("width", maxNodeWidth / 2 - 2)
                    .attr("height", 60)
                    .attr("fill", "white")
                    .attr("class", "ibarsbg")
                    .attr("style", "stroke:#000000")
                    .attr("stroke-width", "1px");

            var egressbg = svg.selectAll(".ebarsbg")
                    .data(stats)
                    .enter()
                    .append("rect")
                    .attr("x", function (d, i) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        return Number(vcp.attr("x")) + maxNodeWidth/2 + 2;
                    })
                    .attr("y", function (d, i) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        if (vcp.attr("y") < height/2)
                            return Number(vcp.attr("y")) + blockHeight;
                        else
                            return Number(vcp.attr("y")) - 60;
                    })
                    .attr("width", maxNodeWidth / 2 - 2)
                    .attr("height", 60)
                    .attr("class", "ebarsbg")
                    .attr("fill", "white")
                    .attr("style", "stroke:#000000")
                    .attr("stroke-width", "1px");

            // Draw bars
            var ingressbars = svg.selectAll(".bars-ingress")
                    .data(stats)
                    .enter()
                    .append("rect")
                    .attr("x", function (d, i) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        return Number(vcp.attr("x")) + 1;
                    })
                    .attr("class", "bars-ingress")
                    .attr("y", function (d, i) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        var speed = vcp.data()[0].speed;
                        if (speed) {
                            var dy = (1 - d.in/(Number(speed)*1000000))*60;
                            if (vcp.attr("y") < height/2) {
                                return Number(vcp.attr("y")) + blockHeight + dy;
                            } else {
                                return Number(vcp.attr("y")) - 60 + dy;
                            }
                        }
                    })
                    .attr("width", maxNodeWidth / 2 - 4)
                    .attr("height", function (d) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        var speed = vcp.data()[0].speed;
                        return d.in/(Number(speed)*1000000)*60;
                    })
                    .attr("fill", "blue")
                    .append("title")
                    .attr("class", "igper")
                    .text(function (d) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        var speed = vcp.data()[0].speed;
                        return parseFloat(d.in/(Number(speed)*1000000)*100).toFixed(2)
                                            + "%" + "\nIngress";
                    });

            var egressbars = svg.selectAll(".bars-egress")
                    .data(stats)
                    .enter()
                    .append("rect")
                    .attr("class", "bars-egress")
                    .attr("x", function (d, i) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                        return d1;
                        });
                        return Number(vcp.attr("x")) + maxNodeWidth/2 + 3;
                    })
                    .attr("y", function (d, i) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        var speed = Number(vcp.data()[0].speed);
                        if (speed) {
                            var dy = (1 - d.out/(Number(speed)*1000000))*60;
                            if (vcp.attr("y") < height/2) {
                                return Number(vcp.attr("y")) + blockHeight + dy;
                            } else {
                                return Number(vcp.attr("y")) - 60 + dy;
                            }
                        }
                    })
                    .attr("width", maxNodeWidth / 2 - 4)
                    .attr("height", function (d) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        var speed = vcp.data()[0].speed;
                        return d.out/(Number(speed)*1000000)*60;
                    })
                    .attr("fill", "blue")
                    .append("title")
                    .append("class", "egper")
                    .text(function (d) {
                        var vcp = d3.selectAll("rect").filter(function (d1) {
                            if (d.name === d1.name)
                                return d1;
                        });
                        var speed = vcp.data()[0].speed;
                        return parseFloat(d.out/(Number(speed)*1000000)*100).toFixed(2)
                                            + "%" + "\nEgress";
                    });

        }   

        /*
         * Finds and updates the data in statistics bars
         */
        function updateBars(stats) {
            stats = JSON.parse(stats).stats;
            if (!stats)
                return;

            var ingressbars = svg.selectAll(".bars-ingress")
                            .data(stats)
                            .transition()
                            .duration(700)
                            .attr("y", function (d, i) {
                                var vcp = d3.selectAll("rect").filter(function (d1) {
                                    if (d.name === d1.name)
                                        return d1;
                                });
                                var speed = vcp.data()[0].speed;
                                if (speed) {
                                    var dy = (1 - d.in/(Number(speed)*1000000))*60;
                                if (vcp.attr("y") < height/2) {
                                    return Number(vcp.attr("y")) + blockHeight + dy;
                                } else {
                                    return Number(vcp.attr("y")) - 60 + dy;
                                }
                                }
                            })
                            .attr("height", function (d) {
                                var vcp = d3.selectAll("rect").filter(function (d1) {
                                    if (d.name === d1.name)
                                        return d1;
                                });
                                var speed = vcp.data()[0].speed;
                                return d.in/(Number(speed)*1000000)*60;
                            });

            var egressbars = svg.selectAll(".bars-egress")
                            .data(stats)
                            .transition()
                            .duration(700)
                            .attr("y", function (d, i) {
                                var vcp = d3.selectAll("rect").filter(function (d1) {
                                    if (d.name === d1.name)
                                        return d1;
                                });
                                var speed = vcp.data()[0].speed;
                                if (speed) {
                                    var dy = (1 - d.out/(Number(speed)*1000000))*60;
                                if (vcp.attr("y") < height/2) {
                                    return Number(vcp.attr("y")) + blockHeight + dy;
                                } else {
                                    return Number(vcp.attr("y")) - 60 + dy;
                                }
                                }
                            })
                            .attr("height", function (d) {
                                var vcp = d3.selectAll("rect").filter(function (d1) {
                                    if (d.name === d1.name)
                                        return d1;
                                });
                                var speed = vcp.data()[0].speed;
                                return d.out/(Number(speed)*1000000)*60;
                            });

            // Update stats percentage
            var igper = svg.selectAll(".igper")
                            .data(stats)
                            .text(function (d) {
                                var vcp = d3.selectAll("rect").filter(function (d1) {
                                    if (d.name === d1.name)
                                        return d1;
                                });
                                var speed = vcp.data()[0].speed;
                                return parseFloat(d.in/(Number(speed)*1000000)*100)
                                        .toFixed(2) + "%" + "\nIngress";
                            });
                               
            var egper = svg.selectAll(".egper")
                            .data(stats)
                            .text(function (d) {
                                var vcp = d3.selectAll("rect").filter(function (d1) {
                                    if (d.name === d1.name)
                                        return d1;
                                });
                                var speed = vcp.data()[0].speed;
                                return parseFloat(d.out/(Number(speed)*1000000)*100)
                                        .toFixed(2) + "%" + "\nEgress";
                            });

        }


        /*
         * Given startX and startY, will draw fpc along with text
         */
        function drawFPC(nodes, startX, startY, spine) {
            // Spines
            var fpcs = svg.selectAll("rect")
                        .data(nodes, function (d) { return d.name; })
                        .enter()
                        .append("rect")
                        .attr("x", function (d, i) {
                            var lpad = startX;
                            var j = i;
                            if (i > 0) {
                                i--;
                                while (i >= 0) {
                                    lpad += nodes[i].children
                                                .length*(maxNodeWidth + space);
                                    i--;
                                }
                            }
                            nodes[j].lpad = lpad;
                            return lpad; 
                        })
                        .attr("y", function (d, i) { 
                            return startY;
                        })
                        .attr("height", 100)
                        .attr("width", function (d) { 
                            return d.children.length * maxNodeWidth 
                                + (d.children.length - 1)*space; 
                        })
                        .attr("style", "stroke:#000000")
                        .attr("text", function (d) { return d.name; })
                        .attr("fill", "orange")
                        .attr("height", blockHeight);

            var labels = svg.selectAll("text")
                    .data(nodes, function (d) { 
                        return d.name; 
                    })
                    .enter()
                    .append("text")
                    .text(function (d) { 
                        return d.name; 
                    })
                    .attr("text-anchor", "middle")
                    .attr("x", function (d, i) {
                        var lpad = startX;
                        if (i > 0) {
                            i--;
                            while (i >= 0) {
                                lpad += nodes[i]
                                        .children
                                        .length*(maxNodeWidth + space);
                                i--;
                            }
                        }
                        return lpad + (d.children.length*maxNodeWidth)/2 
                                    + space;
                    })
                    .attr("y", function (d, i) {
                        return startY + padding; 
                    })
                    .attr("font-family", "sans-serif")
                    .attr("font-size", function (d) {
                        return Math.min(18, (maxNodeWidth - 8) 
                                / this.getComputedTextLength() * 24) + "px";
                    })
                    .attr("fill", "black");

            // Virtual
            for (var i = 0; i < nodes.length; i++) {
                var lpad = nodes[i].lpad;
                var vpcs = svg.selectAll("rect")
                            .data(nodes[i].children, function (d) { 
                                return d.name; 
                            })
                            .enter()
                            .append("rect")
                            .attr("x", function (d, i) {
                                return lpad + i*maxNodeWidth + (i)*space;
                            })
                            .attr("y", function () {
                                if (spine)
                                    return startY + blockHeight + space;
                                else 
                                    return startY - blockHeight - space;
                            })
                            .attr("fill", function (d) {
                                if (d.status === "Up")
                                    return "green";
                                else if (d.status === "Absent")
                                    return "grey";
                                else if (d.status === "Configured")
                                    return "white";
                                else
                                    return "red";
                            })
                            .attr("width", maxNodeWidth)
                            .attr("style", "stroke:#000000")
                            .attr("height", blockHeight)
                            .append("title")
                            .text(function (d) { 
                                return d.name + "\n" 
                                        + d.type + "\n"
                                        + d.status;
                            });

                var vlabels = svg.selectAll("text")
                                .data(nodes[i].children, function (d) {
                                    return d.name; 
                                })
                                .enter()
                                .append("text")
                                .text(function (d) { 
                                    return d.name; 
                                })
                                .attr("x", function (d, i) {
                                    return lpad + i*(maxNodeWidth + space) 
                                                + maxNodeWidth/2;
                                })
                                .attr("text-anchor", "middle")
                                .attr("font-family", "sans-serif")
                                .attr("font-size", function (d) {
                                    return Math.min(18,
                                            (maxNodeWidth - 10) /
                                            this.getComputedTextLength() * 24) 
                                            + "px";
                                })
                                .attr("y", function () {
                                    if (spine)
                                        return startY + 2*blockHeight - space;
                                    else
                                        return startY - blockHeight/2;
                                })
                                .append("title")
                                .text(function (d) { return d.name; });
            }
        }

        function drawConnections (links) {
            var i,
                uniqueLinks = [];

            // Remove repeating links
            for (i = 0; i < links.length; i++) {
                for (j = i - 1; j >= 0; j--) {
                    if (links[i].from === links[j].to 
                            && links[i].to === links[j].from)
                        break;
                }
                if (j < 0) {
                    uniqueLinks.push(links[i]);
                }
            }
            links = uniqueLinks;

            var lines = svg.selectAll("path")
                            .data(links, function (d) { return d.from + d.to; })
                            .enter()
                            .append("path")
                            .attr("d", function (d) {
                                var from = d3.selectAll("rect")
                                            .filter(function (d1) {
                                    if (d1.name === d.from)
                                        return d1;
                                });
                                var to = d3.selectAll("rect")
                                            .filter(function (d1) {
                                    if (d1.name === d.to)
                                        return d1;
                                });

                                var x1 = Number(from.attr('x')) + maxNodeWidth/2;
                                var y1 = Number(from.attr('y')) + blockHeight;

                                var x2 = Number(to.attr('x')) + maxNodeWidth/2;
                                var y2 = Number(to.attr('y'));

                                if (d.type === "intra") {
                                    if (y1 > height / 2)
                                        y1 = y2;
                                    else
                                        y1 = y2 = y2 + blockHeight;
                                }

                                return "M" + x1 + "," + y1 + " Q" 
                                            +  (x1+x2)/2 + "," 
                                            + (height/2) + " " 
                                            + x2 + "," + y2;
                            })
                            .attr("stroke-width", 2)
                            .attr("stroke", function (d) {
                                if (d.type === "intra")
                                    return "brown";
                                return "gray";
                            })
                            .on("mouseover", function (d) {
                                d3.select(this).attr("stroke", "blue");
                                d3.select(this).attr("stroke-width", 3);
                            })
                            .attr("fill", "none")
                            .on("mouseout", function (d) {
                                if (d.type === "intra")
                                    d3.select(this).attr("stroke", "brown");
                                else
                                    d3.select(this).attr("stroke", "gray");
                                d3.select(this).attr("stroke-width", 2);
                            })
                            .append("title")
                            .text(function (d) {
                                return d.from + " -> " + d.to;
                            });
        }
    }
});
