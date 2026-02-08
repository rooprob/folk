#!/usr/bin/env tclsh
# print-tag-list.tcl
# Generate a printable list of tags needed for a booklet

proc printTagList {bookletName pageCount startTagId} {
    puts "==============================================="
    puts "  AprilTags Needed for: $bookletName"
    puts "==============================================="
    puts ""

    set currentTag [expr {$startTagId - 1}]
    puts "Cover Page:"
    puts "  Tag $currentTag → $bookletName-cover"
    puts ""

    set currentTag $startTagId
    puts "Page Spreads:"

    for {set day 1} {$day <= $pageCount} {incr day} {
        set dayStr [format %02d $day]

        puts ""
        puts "Day $dayStr:"
        puts "  Tag $currentTag → $bookletName-$dayStr-L (Left page)"
        incr currentTag
        puts "  Tag $currentTag → $bookletName-$dayStr-R (Right page)"
        incr currentTag
    }

    puts ""
    puts "==============================================="
    puts "Total tags needed: [expr {$currentTag - $startTagId + 1}]"
    puts "Tag range: [expr {$startTagId - 1}] - [expr {$currentTag - 1}]"
    puts "==============================================="
    puts ""
    puts "Generate at: https://april.eecs.umich.edu/software/apriltag.html"
    puts "  - Tag family: 36h11"
    puts "  - Tag IDs: [expr {$startTagId - 1}] through [expr {$currentTag - 1}]"
    puts ""
    puts "Then create mapping:"
    puts "  tclsh generate-tag-mappings.tcl $bookletName $pageCount $startTagId tag-mappings.folk"
}

if {$argc < 3} {
    puts "Usage: tclsh print-tag-list.tcl <booklet-name> <page-count> <start-tag-id>"
    puts ""
    puts "Example:"
    puts "  tclsh print-tag-list.tcl folk-february 28 100"
    puts ""
    exit 1
}

set bookletName [lindex $argv 0]
set pageCount [lindex $argv 1]
set startTagId [lindex $argv 2]

printTagList $bookletName $pageCount $startTagId
