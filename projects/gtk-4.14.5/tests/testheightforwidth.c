/* extendedlayoutexample.c
 * Copyright (C) 2010 Openismus GmbH
 *
 * Author:
 *      Tristan Van Berkom <tristan.van.berkom@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 */

#include <gtk/gtk.h>

typedef struct {
  const char *name;
  const char *tooltip;
  const char *interface;
  GtkWidget   *window;
} TestInterface;


/* These strings were generated with:
 *
 *     IFS=""; while read line; do echo -n \"; echo -n $line | sed -e 's|\"|\\"|g'; echo \"; done < file.glade
 */
static TestInterface interfaces[] = {
  {
    "Ellipsizing Labels",
    "Demonstrates how labels will request a natural size in a horizontal space",
    "<interface>"
    "  <requires lib=\"gtk\" version=\"3.99\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"default_width\">450</property>"
    "    <property name=\"default_height\">50</property>"
    "    <child>"
    "      <object class=\"GtkBox\" id=\"hbox5\">"
    "        <property name=\"orientation\">horizontal</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label9\">"
    "            <property name=\"label\" translatable=\"yes\">Some labels do ellipsize</property>"
    "            <property name=\"ellipsize\">end</property>"
    "            <attributes>"
    "              <attribute name=\"weight\" value=\"bold\"/>"
    "              <attribute name=\"foreground\" value=\"#09610feefe03\"/>"
    "            </attributes>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label10\">"
    "            <property name=\"label\" translatable=\"yes\">but some</property>"
    "            <property name=\"ellipsize\">end</property>"
    "            <attributes>"
    "              <attribute name=\"weight\" value=\"bold\"/>"
    "              <attribute name=\"foreground\" value=\"#0000af6b0993\"/>"
    "            </attributes>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label11\">"
    "            <property name=\"label\" translatable=\"yes\">do not at all</property>"
    "            <attributes>"
    "              <attribute name=\"style\" value=\"normal\"/>"
    "              <attribute name=\"weight\" value=\"bold\"/>"
    "              <attribute name=\"foreground\" value=\"#ffff00000000\"/>"
    "            </attributes>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>",
    NULL
  },

  {
    "Wrapping Label",
    "Demonstrates how a wrapping label can require a height contextual to its allocated width",
    "<interface>"
    "  <requires lib=\"gtk\" version=\"3.99\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"default_width\">300</property>"
    "    <child>"
    "      <object class=\"GtkPaned\" id=\"hpaned1\">"
    "        <property name=\"orientation\">horizontal</property>"
    "        <property name=\"can_focus\">True</property>"
    "        <property name=\"resize-child1\">False</property>"
    "        <property name=\"shrink-child1\">False</property>"
    "        <child>"
    "          <object class=\"GtkBox\" id=\"vbox2\">"
    "            <property name=\"orientation\">vertical</property>"
    "            <child>"
    "              <object class=\"GtkLabel\" id=\"label3\">"
    "                <property name=\"label\" translatable=\"yes\">A short static label.</property>"
    "                <attributes>"
    "                  <attribute name=\"weight\" value=\"bold\"/>"
    "                </attributes>"
    "              </object>"
    "            </child>"
    "            <child>"
    "              <object class=\"GtkFrame\" id=\"frame1\">"
    "                <property name=\"label\">Long label</property>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label1\">"
    "                    <property name=\"label\" translatable=\"yes\">This is a really long label for the purpose of testing line wrapping is working correctly in conjunction with height-for-width support in GTK</property>"
    "                    <property name=\"wrap\">True</property>"
    "                    <property name=\"max_width_chars\">30</property>"
    "                    <attributes>"
    "                      <attribute name=\"foreground\" value=\"#18c52119f796\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "            </child>"
    "            <child>"
    "              <object class=\"GtkButton\" id=\"button2\">"
    "                <property name=\"can_focus\">True</property>"
    "                <property name=\"receives_default\">True</property>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label2\">"
    "                    <property name=\"label\" translatable=\"yes\">A really really long label inside a button to demonstrate height for width working inside buttons</property>"
    "                    <property name=\"wrap\">True</property>"
    "                    <property name=\"max_width_chars\">25</property>"
    "                    <attributes>"
    "                      <attribute name=\"foreground\" value=\"#1e3687ab0a52\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "            </child>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label4\">"
    "            <property name=\"label\" translatable=\"yes\">This static label\n"
    "can shrink.</property>"
    "            <property name=\"justify\">center</property>"
    "            <attributes>"
    "              <attribute name=\"style\" value=\"normal\"/>"
    "              <attribute name=\"foreground\" value=\"#ffff00000000\"/>"
    "            </attributes>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>",
    NULL
  },

  {
    "Horizontal Box",
    "Demonstrates how a horizontal box can calculate the collective height for an allocated width",
    "<interface>"
    "  <requires lib=\"gtk\" version=\"3.99\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"default_height\">200</property>"
    "    <property name=\"default_width\">600</property>"
    "    <child>"
    "      <object class=\"GtkPaned\" id=\"hpaned1\">"
    "        <property name=\"orientation\">horizontal</property>"
    "        <property name=\"can_focus\">True</property>"
    "        <property name=\"resize-child1\">False</property>"
    "        <property name=\"shrink-child1\">False</property>"
    "        <child>"
    "          <object class=\"GtkBox\" id=\"vbox1\">"
    "            <property name=\"orientation\">vertical</property>"
    "            <child>"
    "              <object class=\"GtkBox\" id=\"hbox1\">"
    "                <property name=\"orientation\">horizontal</property>"
    "                <child>"
    "                  <object class=\"GtkButton\" id=\"button1\">"
    "                    <property name=\"can_focus\">True</property>"
    "                    <property name=\"receives_default\">True</property>"
    "                    <child>"
    "                      <object class=\"GtkLabel\" id=\"label2\">"
    "                        <property name=\"label\" translatable=\"yes\">A button that wraps.</property>"
    "                        <property name=\"wrap\">True</property>"
    "                        <property name=\"width_chars\">10</property>"
    "                        <attributes>"
    "                          <attribute name=\"foreground\" value=\"#0000041dffff\"/>"
    "                        </attributes>"
    "                      </object>"
    "                    </child>"
    "                  </object>"
    "                </child>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label1\">"
    "                    <property name=\"label\" translatable=\"yes\">Lets try setting up some long text to wrap up in this hbox and see if the height-for-width is gonna work !</property>"
    "                    <property name=\"wrap\">True</property>"
    "                    <property name=\"width_chars\">30</property>"
    "                    <attributes>"
    "                      <attribute name=\"foreground\" value=\"#07d0a9b20972\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "            </child>"
    "            <child>"
    "              <object class=\"GtkButton\" id=\"button2\">"
    "                <property name=\"label\" translatable=\"yes\">A button that expands in the vbox</property>"
    "                <property name=\"can_focus\">True</property>"
    "                <property name=\"receives_default\">True</property>"
    "              </object>"
    "            </child>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label4\">"
    "            <property name=\"label\" translatable=\"yes\">This label is\n"
    "set to shrink inside\n"
    "the paned window.</property>"
    "            <property name=\"justify\">center</property>"
    "            <attributes>"
    "              <attribute name=\"style\" value=\"normal\"/>"
    "              <attribute name=\"foreground\" value=\"#ffff00000000\"/>"
    "            </attributes>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>",
    NULL
  },

  {
    "Label Parameters",
    "This test demonstrates how \"width-chars\" and \"max-width-chars\" can be used "
    "to effect minimum and natural widths in wrapping labels.",
    "<interface>"
    "  <requires lib=\"gtk\" version=\"3.99\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"default_width\">900</property>"
    "    <child>"
    "      <object class=\"GtkPaned\" id=\"hpaned1\">"
    "        <property name=\"orientation\">horizontal</property>"
    "        <property name=\"can_focus\">True</property>"
    "        <property name=\"resize-child1\">False</property>"
    "        <property name=\"shrink-child1\">False</property>"
    "        <child>"
    "          <object class=\"GtkBox\" id=\"vbox1\">"
    "            <property name=\"orientation\">vertical</property>"
    "            <child>"
    "              <object class=\"GtkBox\" id=\"hbox1\">"
    "                <property name=\"orientation\">horizontal</property>"
    "                <property name=\"spacing\">6</property>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label1\">"
    "                    <property name=\"label\" translatable=\"yes\">The first 2 labels require 10 characters.</property>"
    "                    <property name=\"wrap\">True</property>"
    "                    <property name=\"width_chars\">10</property>"
    "                    <attributes>"
    "                      <attribute name=\"weight\" value=\"bold\"/>"
    "                      <attribute name=\"foreground\" value=\"#ffff00000000\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label2\">"
    "                    <property name=\"label\" translatable=\"yes\">This label has a maximum natural width of 20 characters. The second two labels expand.</property>"
    "                    <property name=\"wrap\">True</property>"
    "                    <property name=\"width_chars\">10</property>"
    "                    <property name=\"max_width_chars\">20</property>"
    "                    <attributes>"
    "                      <attribute name=\"weight\" value=\"bold\"/>"
    "                      <attribute name=\"foreground\" value=\"#05c2a161134b\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label3\">"
    "                    <property name=\"label\" translatable=\"yes\">This label requires a default minimum size.</property>"
    "                    <property name=\"wrap\">True</property>"
    "                    <attributes>"
    "                      <attribute name=\"weight\" value=\"bold\"/>"
    "                      <attribute name=\"foreground\" value=\"#03e30758fb5f\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "            </child>"
    "            <child>"
    "              <object class=\"GtkLabel\" id=\"label4\">"
    "                <property name=\"label\" translatable=\"yes\">This test demonstrates how the \"width-chars\" and \"max-width-chars\"\n"
    "properties can be used to specify the minimum requested wrap width\n"
    "and the maximum natural wrap width respectively.</property>"
    "                <property name=\"ellipsize\">end</property>"
    "                <property name=\"width_chars\">30</property>"
    "                <attributes>"
    "                  <attribute name=\"style\" value=\"normal\"/>"
    "                  <attribute name=\"foreground\" value=\"#05470000abaf\"/>"
    "                </attributes>"
    "              </object>"
    "            </child>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label5\">"
    "            <property name=\"label\" translatable=\"yes\">Some static\n"
    "text that shrinks.\n"
    "\n"
    "You will need to stretch\n"
    "this window quite wide\n"
    "to see the effects.</property>"
    "            <property name=\"justify\">center</property>"
    "            <attributes>"
    "              <attribute name=\"foreground\" value=\"#ffff00000000\"/>"
    "            </attributes>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>",
    NULL
  },

  {
    "Wrapping Expander",
    "This test demonstrates how the expander label can fill to its natural width "
    "and also trade height for width.",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<interface>"
    "  <requires lib=\"gtk\" version=\"3.99\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"default_width\">500</property>"
    "    <child>"
    "      <object class=\"GtkPaned\" id=\"hpaned1\">"
    "        <property name=\"orientation\">horizontal</property>"
    "        <property name=\"can_focus\">True</property>"
    "        <property name=\"resize-child1\">False</property>"
    "        <property name=\"shrink-child1\">False</property>"
    "        <child>"
    "          <object class=\"GtkExpander\" id=\"expander1\">"
    "            <property name=\"can_focus\">True</property>"
    "            <child>"
    "              <object class=\"GtkLabel\" id=\"label2\">"
    "                <property name=\"label\" translatable=\"yes\">More wrapping text to fill the largish content area in the expander </property>"
    "                <property name=\"wrap\">True</property>"
    "                <property name=\"width_chars\">10</property>"
    "                <attributes>"
    "                  <attribute name=\"weight\" value=\"bold\"/>"
    "                  <attribute name=\"foreground\" value=\"#0000D0F00000\"/>"
    "                </attributes>"
    "              </object>"
    "            </child>"
    "            <child type=\"label\">"
    "              <object class=\"GtkLabel\" id=\"label1\">"
    "                <property name=\"label\" translatable=\"yes\">Here is some expander text that wraps</property>"
    "                <property name=\"wrap\">True</property>"
    "                <property name=\"width_chars\">10</property>"
    "                <attributes>"
    "                  <attribute name=\"weight\" value=\"bold\"/>"
    "                  <attribute name=\"foreground\" value=\"blue\"/>"
    "                </attributes>"
    "              </object>"
    "            </child>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label3\">"
    "            <property name=\"label\" translatable=\"yes\">static\n"
    "text\n"
    "here</property>"
    "            <attributes>"
    "              <attribute name=\"foreground\" value=\"red\"/>"
    "            </attributes>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>",
    NULL
  },

  {
    "Wrapping Frame Label",
    "This test demonstrates how the frame label can fill to its natural width "
    "and also trade height for width.",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<interface>"
    "  <requires lib=\"gtk\" version=\"3.99\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"default_width\">400</property>"
    "    <property name=\"default_height\">150</property>"
    "    <child>"
    "      <object class=\"GtkFrame\" id=\"frame1\">"
    "        <property name=\"label_xalign\">0</property>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label2\">"
    "            <property name=\"margin_start\">12</property>"
    "            <property name=\"label\" translatable=\"yes\">some content</property>"
    "          </object>"
    "        </child>"
    "        <child type=\"label\">"
    "          <object class=\"GtkLabel\" id=\"label1\">"
    "            <property name=\"label\" translatable=\"yes\">A frame label that's a little long and wraps</property>"
    "            <property name=\"use_markup\">True</property>"
    "            <property name=\"wrap\">True</property>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "</interface>",
    NULL
  },

  {
    "Combo Boxes and Menus",
    "This test shows wrapping and ellipsizing text in combo boxes (and consequently in menu items).",
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<interface>"
    "  <requires lib=\"gtk\" version=\"3.99\"/>"
    "  <!-- interface-naming-policy project-wide -->"
    "  <object class=\"GtkWindow\" id=\"window\">"
    "    <property name=\"default_width\">600</property>"
    "    <child>"
    "      <object class=\"GtkPaned\" id=\"hpaned1\">"
    "        <property name=\"orientation\">horizontal</property>"
    "        <property name=\"can_focus\">True</property>"
    "        <property name=\"shrink-child1\">False</property>"
    "        <property name=\"resize-child2\">False</property>"
    "        <child>"
    "          <object class=\"GtkBox\" id=\"vbox1\">"
    "            <property name=\"orientation\">vertical</property>"
    "            <property name=\"spacing\">5</property>"
    "            <child>"
    "              <object class=\"GtkBox\" id=\"hbox1\">"
    "                <property name=\"orientation\">horizontal</property>"
    "                <property name=\"spacing\">5</property>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label1\">"
    "                    <property name=\"label\" translatable=\"yes\">this combo box</property>"
    "                    <attributes>"
    "                      <attribute name=\"weight\" value=\"bold\"/>"
    "                      <attribute name=\"foreground\" value=\"#b3460000eb1c\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label2\">"
    "                    <property name=\"label\" translatable=\"yes\">contains some wrapping locations</property>"
    "                    <property name=\"ellipsize\">end</property>"
    "                    <property name=\"width_chars\">10</property>"
    "                    <attributes>"
    "                      <attribute name=\"weight\" value=\"bold\"/>"
    "                      <attribute name=\"foreground\" value=\"#b3460000eb1c\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "                <child>"
    "                  <object class=\"GtkComboBox\" id=\"combobox1\">"
    "                    <property name=\"model\">liststore1</property>"
    "                    <property name=\"active\">0</property>"
    "                    <child>"
    "                      <object class=\"GtkCellRendererPixbuf\" id=\"cellrenderertext1\"/>"
    "                      <attributes>"
    "                        <attribute name=\"icon-name\">1</attribute>"
    "                      </attributes>"
    "                    </child>"
    "                    <child>"
    "                      <object class=\"GtkCellRendererText\" id=\"cellrenderertext2\">"
    "                        <property name=\"foreground\">purple</property>"
    "                        <property name=\"weight\">600</property>"
    "                        <property name=\"wrap_mode\">word</property>"
    "                        <property name=\"wrap_width\">100</property>"
    "                      </object>"
    "                      <attributes>"
    "                        <attribute name=\"text\">0</attribute>"
    "                      </attributes>"
    "                    </child>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "            </child>"
    "            <child>"
    "              <object class=\"GtkFrame\" id=\"frame1\">"
    "                <property name=\"label_xalign\">0</property>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label3\">"
    "                    <property name=\"label\" translatable=\"yes\">This test shows combo boxes\n"
    "requesting and allocating space\n"
    "for its backing content using\n"
    "height-for-width geometry\n"
    "management.\n"
    "\n"
    "Note this test also demonstrates\n"
    "height-for-width menu items.</property>"
    "                    <property name=\"justify\">center</property>"
    "                    <attributes>"
    "                      <attribute name=\"weight\" value=\"bold\"/>"
    "                      <attribute name=\"foreground\" value=\"#00000000ffff\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "            </child>"
    "            <child>"
    "              <object class=\"GtkBox\" id=\"hbox2\">"
    "                <property name=\"orientation\">horizontal</property>"
    "                <property name=\"spacing\">5</property>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label4\">"
    "                    <property name=\"label\" translatable=\"yes\">this combo box</property>"
    "                    <attributes>"
    "                      <attribute name=\"weight\" value=\"bold\"/>"
    "                      <attribute name=\"foreground\" value=\"#ffffa5a50000\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "                <child>"
    "                  <object class=\"GtkLabel\" id=\"label5\">"
    "                    <property name=\"label\" translatable=\"yes\">contains some ellipsizing locations</property>"
    "                    <property name=\"ellipsize\">end</property>"
    "                    <property name=\"width_chars\">10</property>"
    "                    <attributes>"
    "                      <attribute name=\"weight\" value=\"bold\"/>"
    "                      <attribute name=\"foreground\" value=\"#ffffa5a50000\"/>"
    "                    </attributes>"
    "                  </object>"
    "                </child>"
    "                <child>"
    "                  <object class=\"GtkComboBox\" id=\"combobox2\">"
    "                    <property name=\"model\">liststore1</property>"
    "                    <property name=\"active\">0</property>"
    "                    <child>"
    "                      <object class=\"GtkCellRendererPixbuf\" id=\"cellrenderertext3\"/>"
    "                      <attributes>"
    "                        <attribute name=\"icon-name\">1</attribute>"
    "                      </attributes>"
    "                    </child>"
    "                    <child>"
    "                      <object class=\"GtkCellRendererText\" id=\"cellrenderertext4\">"
    "                        <property name=\"ellipsize\">end</property>"
    "                        <property name=\"foreground\">orange</property>"
    "                        <property name=\"weight\">600</property>"
    "                        <property name=\"width_chars\">10</property>"
    "                      </object>"
    "                      <attributes>"
    "                        <attribute name=\"text\">0</attribute>"
    "                      </attributes>"
    "                    </child>"
    "                  </object>"
    "                </child>"
    "              </object>"
    "            </child>"
    "          </object>"
    "        </child>"
    "        <child>"
    "          <object class=\"GtkLabel\" id=\"label6\">"
    "            <property name=\"label\" translatable=\"yes\">Some static\n"
    "text here\n"
    "that shrinks.</property>"
    "            <property name=\"justify\">center</property>"
    "            <attributes>"
    "              <attribute name=\"foreground\" value=\"#ffff00000000\"/>"
    "            </attributes>"
    "          </object>"
    "        </child>"
    "      </object>"
    "    </child>"
    "  </object>"
    "  <object class=\"GtkListStore\" id=\"liststore1\">"
    "    <columns>"
    "      <!-- column-name item-text -->"
    "      <column type=\"gchararray\"/>"
    "      <!-- column-name icon-name -->"
    "      <column type=\"gchararray\"/>"
    "    </columns>"
    "    <data>"
    "      <row>"
    "        <col id=\"0\" translatable=\"yes\">Montreal, Quebec Canada</col>"
    "        <col id=\"1\" translatable=\"yes\">edit-undo</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\" translatable=\"yes\">Sao Paulo, SP Brazil</col>"
    "        <col id=\"1\" translatable=\"yes\">edit-redo</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\" translatable=\"yes\">Buenos Aires, Argentina</col>"
    "        <col id=\"1\" translatable=\"yes\">process-stop</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\" translatable=\"yes\">Los Angelos, California USA</col>"
    "        <col id=\"1\" translatable=\"yes\">media-record</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\" translatable=\"yes\">Rio de Janeiro, RJ Brazil</col>"
    "        <col id=\"1\" translatable=\"yes\">dialog-error</col>"
    "      </row>"
    "      <row>"
    "        <col id=\"0\" translatable=\"yes\">Seoul, South Korea</col>"
    "        <col id=\"1\" translatable=\"yes\">dialog-information</col>"
    "      </row>"
    "    </data>"
    "  </object>"
    "</interface>",
    NULL
  },
  
};


static void
test_clicked (GtkWidget     *button, 
              TestInterface *interface)
{
  if (!interface->window)
    {
      GtkBuilder *builder = gtk_builder_new ();
      GError *error = NULL;
      
      gtk_builder_add_from_string (builder, interface->interface, -1, &error);

      if (error)
        {
          g_printerr ("GtkBuilder for interface \"%s\" returned error \"%s\"\n",
                      interface->name, error->message);

          g_error_free (error);
          g_object_unref (builder);

          return;
        }

      interface->window = (GtkWidget *)gtk_builder_get_object (builder, "window");

      gtk_window_set_hide_on_close (GTK_WINDOW (interface->window), TRUE);

      g_object_unref (builder);
    }

  gtk_window_present (GTK_WINDOW (interface->window));
}


static GtkWidget *
create_window (void)
{
  GtkWidget *window, *vbox, *button;
  int i;

  window = gtk_window_new ();
  vbox   = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

  gtk_widget_set_margin_start (vbox, 8);
  gtk_widget_set_margin_end (vbox, 8);
  gtk_widget_set_margin_top (vbox, 8);
  gtk_widget_set_margin_bottom (vbox, 8);

  gtk_window_set_child (GTK_WINDOW (window), vbox);

  for (i = 0; i < G_N_ELEMENTS (interfaces); i++)
    {
      button = gtk_button_new_with_label (interfaces[i].name);

      gtk_widget_set_tooltip_text (button, interfaces[i].tooltip);

      g_signal_connect (button, "clicked",
                        G_CALLBACK (test_clicked), &interfaces[i]);

      gtk_box_append (GTK_BOX (vbox), button);
    }

  return window;
}

static void
quit_cb (GtkWidget *widget,
         gpointer   data)
{
  gboolean *done = data;

  *done = TRUE;

  g_main_context_wakeup (NULL);
}

int
main (int argc, char *argv[])
{
  GtkWidget *window;
  gboolean done = FALSE;

  gtk_init ();

  window = create_window ();

  g_signal_connect (window, "destroy", G_CALLBACK (quit_cb), &done);

  gtk_window_present (GTK_WINDOW (window));

  while (!done)
    g_main_context_iteration (NULL, TRUE);

  return 0;
}
