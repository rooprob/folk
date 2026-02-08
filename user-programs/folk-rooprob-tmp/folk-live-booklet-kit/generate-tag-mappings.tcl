#!/usr/bin/env tclsh
# generate-tag-mappings.tcl
# Helper script to generate tag mapping files for booklets

proc generateMappings {bookletName pageCount startTagId outputFile} {
    set fp [open $outputFile w]

    puts $fp "# tag-mappings.folk"
    puts $fp "# Auto-generated tag mappings for $bookletName booklet"
    puts $fp "# Generated on [clock format [clock seconds]]"
    puts $fp ""
    puts $fp "set this tag-mappings"
    puts $fp ""
    puts $fp "# Physical tag IDs: $startTagId to [expr {$startTagId + $pageCount * 2 - 1}]"
    puts $fp "# Logical pages: $bookletName-01 through $bookletName-[format %02d $pageCount]"
    puts $fp ""

    set currentTag $startTagId

    for {set day 1} {$day <= $pageCount} {incr day} {
        set dayStr [format %02d $day]

        puts $fp "# Day $dayStr"
        puts $fp "Claim tag $currentTag is page $bookletName-$dayStr-L"
        incr currentTag
        puts $fp "Claim tag $currentTag is page $bookletName-$dayStr-R"
        incr currentTag
        puts $fp ""
    }

    puts $fp "# Cover page"
    puts $fp "Claim tag [expr {$startTagId - 1}] is page $bookletName-cover"
    puts $fp ""

    puts $fp "puts \"Tag mappings loaded: $bookletName (tags $startTagId-[expr {$currentTag-1}])\""

    close $fp

    puts "Generated $outputFile"
    puts "  Booklet: $bookletName"
    puts "  Pages: $pageCount"
    puts "  Tag range: $startTagId - [expr {$currentTag - 1}]"
    puts ""
    puts "Edit this file to match your printed tags!"
}

# Usage examples
if {$argc < 4} {
    puts "Usage: tclsh generate-tag-mappings.tcl <booklet-name> <page-count> <start-tag-id> <output-file>"
    puts ""
    puts "Example:"
    puts "  tclsh generate-tag-mappings.tcl folk-february 28 100 tag-mappings.folk"
    puts ""
    puts "This generates mappings for:"
    puts "  - Booklet: folk-february"
    puts "  - 28 days (56 pages total)"
    puts "  - Using tags 100-155"
    puts ""
    exit 1
}

set bookletName [lindex $argv 0]
set pageCount [lindex $argv 1]
set startTagId [lindex $argv 2]
set outputFile [lindex $argv 3]

generateMappings $bookletName $pageCount $startTagId $outputFile

puts "Next steps:"
puts "  1. Print AprilTags $startTagId through [expr {$startTagId + $pageCount * 2 - 1}]"
puts "  2. Review and adjust $outputFile if needed"
puts "  3. Load the booklet: source load-folk-february.folk"
