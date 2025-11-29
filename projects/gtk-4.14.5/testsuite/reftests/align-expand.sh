#!/bin/bash
#
# align-expand.sh [METHOD]
#
# This is the script used to create the align-expand tests. These tests
# put a 20x20 size-requested GtkTreeView into a 40x40 size-requested
# container and try to achieve multiple combinations of expand and align
# flags. The resulting GtkBuilder file is written to stdout. All of the
# resulting files should render identical.
#
# METHOD is one of:
# * flags (default)
#   Uses expand flags to align and expand the treeview inside a GtkGrid.
#   You should use this as the reference when adding tests for other
#   methods
# * alignment
#   Aligns and expands the treeview in a GtkAlignment using its scale
#   and align properties.


if test $# -lt 1; then
  method="flags"
else
  method=$1
fi

cat << EOF
<?xml version="1.0" encoding="UTF-8"?>
<interface>
  <!-- interface-requires gtk+ 3.0 -->
  <object class="GtkWindow" id="window">
    <property name="can_focus">False</property>
    <property name="decorated">0</property>
    <child>
      <object class="GtkGrid" id="grid">
        <property name="visible">True</property>
        <property name="can_focus">False</property>
        <property name="row_spacing">2</property>
        <property name="column_spacing">2</property>
EOF


y=2
for hexpand in False True; do
for halign in "start" center end fill; do

cat << EOF
        <child>
          <object class="GtkLabel" id="hexpand-$halign-$hexpand">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="label">$hexpand</property>
          </object>
          <packing>
            <property name="left_attach">$y</property>
            <property name="top_attach">0</property>
            <property name="width">1</property>
            <property name="height">1</property>
          </packing>
        </child>
        <child>
          <object class="GtkLabel" id="halign-$halign-$hexpand">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="label">$halign</property>
          </object>
          <packing>
            <property name="left_attach">$y</property>
            <property name="top_attach">1</property>
            <property name="width">1</property>
            <property name="height">1</property>
          </packing>
        </child>
EOF
x=2
for vexpand in False True; do
for valign in "start" center end fill; do

if test $y = "2"; then
cat << EOF
        <child>
          <object class="GtkLabel" id="vexpand-$valign-$vexpand">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="label">$vexpand</property>
          </object>
          <packing>
            <property name="left_attach">0</property>
            <property name="top_attach">$x</property>
            <property name="width">1</property>
            <property name="height">1</property>
          </packing>
        </child>
        <child>
          <object class="GtkLabel" id="valign-$valign-$vexpand">
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="label">$valign</property>
          </object>
          <packing>
            <property name="left_attach">1</property>
            <property name="top_attach">$x</property>
            <property name="width">1</property>
            <property name="height">1</property>
          </packing>
        </child>
EOF
fi

if test $method = "flags"; then

cat << EOF
        <child>
          <object class="GtkGrid" id="grid-$valign-$halign-$vexpand-$hexpand">
            <property name="width_request">40</property>
            <property name="height_request">40</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <child>
              <object class="GtkTreeView" id="treeview-$valign-$halign-$vexpand-$hexpand">
                <property name="width_request">20</property>
                <property name="height_request">20</property>
                <property name="visible">True</property>
                <property name="can_focus">False</property>
                <property name="halign">$halign</property>
                <property name="valign">$valign</property>
                <property name="hexpand">$hexpand</property>
                <property name="vexpand">$vexpand</property>
              </object>
              <packing>
                <property name="left_attach">0</property>
                <property name="top_attach">0</property>
                <property name="width">1</property>
                <property name="height">1</property>
              </packing>
            </child>
          </object>
          <packing>
            <property name="left_attach">$x</property>
            <property name="top_attach">$y</property>
            <property name="width">1</property>
            <property name="height">1</property>
          </packing>
        </child>
EOF
  
elif test $method = "alignment"; then

xscale=0.0
case "$halign" in
  "start") xalign=0.0 ;;
  "center") xalign=0.5 ;;
  "end") xalign=1.0 ;;
  "fill") xalign=0.5; xscale=1.0 ;;
esac
if test $hexpand = "True"; then
  xscale=1.0
fi

yscale=0.0
case "$valign" in
  "start") yalign=0.0 ;;
  "center") yalign=0.5 ;;
  "end") yalign=1.0 ;;
  "fill") yalign=0.5; yscale=1.0 ;;
esac
if test $vexpand = "True"; then
  yscale=1.0
fi

cat << EOF
        <child>
          <object class="GtkAlignment" id="align-$valign-$halign-$vexpand-$hexpand">
            <property name="width_request">40</property>
            <property name="height_request">40</property>
            <property name="visible">True</property>
            <property name="can_focus">False</property>
            <property name="xalign">$xalign</property>
            <property name="yalign">$yalign</property>
            <property name="xscale">$xscale</property>
            <property name="yscale">$yscale</property>
            <child>
              <object class="GtkTreeView" id="treeview-$valign-$halign-$vexpand-$hexpand">
                <property name="width_request">20</property>
                <property name="height_request">20</property>
                <property name="visible">True</property>
                <property name="can_focus">False</property>
              </object>
            </child>
          </object>
          <packing>
            <property name="left_attach">$x</property>
            <property name="top_attach">$y</property>
            <property name="width">1</property>
            <property name="height">1</property>
          </packing>
        </child>
EOF

else

  exit 1

fi

x=`expr $x + 1`
done
done

y=`expr $y + 1`
x=0
done
done

cat << EOF
      </object>
    </child>
  </object>
</interface>
EOF
