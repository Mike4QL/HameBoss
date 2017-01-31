#include <gtk/gtk.h>
#include <stdint.h>
#include <stdlib.h>

GtkWidget *window, *btnDo1, *btnDo2, *btnExit, *table, *vbox, *frame, *entryAddr, *entryValue, *display, *aspect_frame, *test;

static void do1 (GtkWidget *widget, gpointer data){			// what happens on button click
	gtk_label_set_text(data, "insert new text to check how it is displayed");
	//uint8_t i;
	//for (i
}
static void do2 (GtkWidget *widget, gpointer data){			// what happens on button click
	gtk_label_set_text(data, "insert new\ntext with a few\nlines");
}
static void enter_text(GtkWidget *widget, gpointer entry){	// what happens on text enter
	const gchar *entry_text;
	entry_text = gtk_entry_get_text (GTK_ENTRY (entry));
	//gtk_label_set_text((gpointer)label, entry_text);
}
	
/* begin linked list */
typedef struct node {
	struct node *next, *prev;
	uint8_t addr, val;
} node;
node* listInsert(node *pos, uint8_t addr, uint8_t val){			// insert after the node
	node *pNew;
	pNew = (node*) malloc (sizeof(node));
	pNew -> addr = addr;
	pNew -> val = val;
	pNew -> next = pos -> next;
	pNew -> prev = pos;
	pNew -> prev -> next = pNew;
	if (pNew -> next)
		pNew -> next -> prev = pNew;
	return pNew;
}
void listRemove(node *pos){
	if (pos -> prev)
		pos -> prev -> next = pos -> next;
	if (pos -> next)
		pos -> next -> prev = pos -> prev;
	free (pos);
	return;
}
node* listFindAddr(node *pos, uint8_t addr){
	while (pos)
		if (pos -> addr == addr)
			break;
		else
			pos = pos -> next;
	return pos;
}
/* end linked list */

int main(int argc, char *argv[])
{
	node *root = (node*) malloc (sizeof(node)), *last;
	root-> next = NULL;
	root-> prev = NULL;
	last = listInsert(root, 0x05, 0x01);
	last = listInsert(last, 0x06, 0xEC);
	last = listInsert(last, 0x07, 0x6C);
	last = listInsert(last, 0x08, 0x93);
	last = listInsert(last, 0x09, 0x33);
	last = listInsert(last, 0x0B, 0x20);
	gtk_init(&argc, &argv);
	window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	
	//gtk_container_set_border_width (GTK_CONTAINER (window), 40);
	
	gtk_window_set_title (GTK_WINDOW (window), "Gateway controller");
	gtk_widget_set_size_request(GTK_WIDGET (window), 400, 300);

    table = gtk_table_new (20, 20, TRUE);
    
    gtk_container_add (GTK_CONTAINER (window), table);
    btnDo1 = gtk_button_new_with_label ("Insert");
    gtk_table_attach_defaults (GTK_TABLE (table), btnDo1, 0 ,4, 3, 5);
    btnDo2 = gtk_button_new_with_label ("Remove");
    gtk_table_attach_defaults (GTK_TABLE (table), btnDo2, 0 ,4, 6, 8);
    btnExit = gtk_button_new_with_label ("Exit");
    gtk_table_attach_defaults (GTK_TABLE (table), btnExit, 0 ,4, 0, 2);
    
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_table_attach_defaults (GTK_TABLE (table), vbox, 8 ,19, 1, 2);
    display = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (display), 150);
    gtk_entry_set_text (GTK_ENTRY (display), "Setup registers\nsomething:");
    gtk_box_pack_start (GTK_BOX (vbox), display, TRUE, TRUE, 0);
    
    
    vbox = gtk_vbox_new (FALSE, 0);    // CONTINUE HERE!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    gtk_table_attach_defaults (GTK_TABLE (table), vbox, 8 ,19, 3, 15);
    test = gtk_text_new ();
    //gtk_entry_set_max_length (GTK_ENTRY (display), 150);
    //gtk_entry_set_text (GTK_ENTRY (display), "Setup registers\nsomething:");
    gtk_box_pack_start (GTK_BOX (vbox), test, TRUE, TRUE, 0);
    
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_table_attach_defaults (GTK_TABLE (table), vbox, 1 ,6, 18, 19);
    entryAddr = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (entryAddr), 50);
    gtk_entry_set_text (GTK_ENTRY (entryAddr), "0xFF");
    gtk_box_pack_start (GTK_BOX (vbox), entryAddr, TRUE, TRUE, 0);
    
    vbox = gtk_vbox_new (FALSE, 0);
    gtk_table_attach_defaults (GTK_TABLE (table), vbox, 7 ,12, 18, 19);
    entryValue = gtk_entry_new ();
    gtk_entry_set_max_length (GTK_ENTRY (entryValue), 50);
    gtk_entry_set_text (GTK_ENTRY (entryValue), "0xFF");
    gtk_box_pack_start (GTK_BOX (vbox), entryValue, TRUE, TRUE, 0);
    
    aspect_frame = gtk_aspect_frame_new ("Reg Addr:", 1, 1, 3, FALSE);
	gtk_table_attach_defaults (GTK_TABLE (table), aspect_frame, 1 ,6, 17, 20);
	aspect_frame = gtk_aspect_frame_new ("Reg Value:", 1, 1, 3, FALSE);
	gtk_table_attach_defaults (GTK_TABLE (table), aspect_frame, 7 ,12, 17, 20);
    
    g_signal_connect (window, "destroy", G_CALLBACK (gtk_main_quit), NULL);	// window close clicked -> destroy
    //g_signal_connect (btnDo1, "clicked", G_CALLBACK (do1), label);		// btn clicked -> do1()
    //g_signal_connect (btnDo2, "clicked", G_CALLBACK (do2), label);		// btn clicked -> do2()
	g_signal_connect_swapped (btnExit, "clicked", G_CALLBACK (gtk_main_quit), window);		// btn clicked -> destroy
	g_signal_connect (entryAddr, "activate", G_CALLBACK (enter_text), entryAddr);
    
    gtk_widget_show_all (window);
    
    gtk_main ();
	
	return 0;
}
