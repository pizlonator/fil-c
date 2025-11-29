#!/usr/bin/env -S GI_TYPELIB_PATH=${PWD}/build/gtk:${GI_TYPELIB_PATH} LD_PRELOAD=${LD_PRELOAD}:${PWD}/build/gtk/libgtk-4.so gjs

imports.gi.versions['Gtk'] = '4.0';

const GObject = imports.gi.GObject;
const Gtk = imports.gi.Gtk;

const DemoWidget = GObject.registerClass({
  GTypeName: 'DemoWidget',
  }, class DemoWidget extends Gtk.Widget {

  _init(params = {}) {
       super._init(params);

       let layout_manager = new Gtk.GridLayout ();
       this.set_layout_manager (layout_manager);
       this.label1 = new Gtk.Label({ label: "Red",
                                     hexpand: true,
                                     vexpand: true });
       this.label1.set_parent (this);
       let child1 = layout_manager.get_layout_child (this.label1);
       child1.set_row (0);
       child1.set_column (0);

       this.label2 = new Gtk.Label({ label: "Green",
                                     hexpand: true,
                                     vexpand: true });
       this.label2.set_parent (this);
       let child2 = layout_manager.get_layout_child (this.label2);
       child2.set_row (0);
       child2.set_column (1);
    }
});

// Create a new application
let app = new Gtk.Application({ application_id: 'org.gtk.exampleapp' });

// When the application is launched…
app.connect('activate', () => {
    // … create a new window …
    let win = new Gtk.ApplicationWindow({ application: app });
    // … with a button in it …
    let widget = new DemoWidget();
    win.set_child(widget);
    win.present();
});

// Run the application
app.run([]);

