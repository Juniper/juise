# Map MPLS LSP 

The Map MPLS LSP CLIRA app uses the Google Maps API and CLIRA API to visualize and plot the location of the devices that are part of the LSP in a MPLS network. 

### Fetaures
  - The app displays the LSP info and a graph showing the devices forming the LSP.
  - The user can click on the node in the graph to view the real time traffic statistics.
  - The user can click on the 'Show LSP on Map' button to view the LSP on the map.

### Assumptions and Prerequisites
  - The location info should be configured on each device using, “set system location” command.
  - The other devices in the LSP should be reachable by host and share the same password.

### Running the app
To invoke the app enter "map mpls lsp" followed by the devicename.

**Dependency**
- [vis.js](http://visjs.org)
