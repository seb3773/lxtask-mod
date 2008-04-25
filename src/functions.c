/* $Id: functions.c 3940 2008-02-10 22:48:45Z nebulon $
 *
 * Copyright (c) 2006 Johannes Zellner, <webmaster@nebulon.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Library General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/* modified by Hong Jen Yee <pcman.tw@gmail.com> and the LXDE team
 * on 2008-04-26 to be used in LXDE.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib.h>
#include <glib/gi18n.h>
#include <stdio.h>

#include "functions.h"

static system_status *sys_stat =NULL;

gboolean refresh_task_list(void)
{
    gint i, j;
    GArray *new_task_list;
    gchar *cpu_tooltip, *mem_tooltip;
    gdouble cpu_usage;
    guint num_cpus;
    guint memory_used;

    if (sys_stat!=NULL)
        num_cpus = sys_stat->cpu_count;
    else
        num_cpus = 1;

    /* gets the new task list */
    new_task_list = (GArray*) get_task_list();

    /* check if task is new and marks the task that its checked*/
    for(i = 0; i < task_array->len; i++)
    {
        struct task *tmp = &g_array_index(task_array, struct task, i);
        tmp->checked = FALSE;

        for(j = 0; j < new_task_list->len; j++)
        {
            struct task *new_tmp = &g_array_index(new_task_list, struct task, j);

            if(new_tmp->pid == tmp->pid)
            {
                tmp->old_time = tmp->time;

                tmp->time = new_tmp->time;

                tmp->time_percentage = (gdouble)(tmp->time - tmp->old_time) * (gdouble)(1000.0 / (REFRESH_INTERVAL*num_cpus));
                if(
                    (gint)tmp->ppid != (gint)new_tmp->ppid ||
                    strcmp(tmp->state,new_tmp->state) ||
                    (unsigned int)tmp->size != (unsigned int)new_tmp->size ||
                    (unsigned int)tmp->rss != (unsigned int)new_tmp->rss ||
                    (unsigned int)tmp->time != (unsigned int)tmp->old_time ||
                    tmp->prio != new_tmp->prio
                 )
                {
                    tmp->ppid = new_tmp->ppid;
                    strcpy(tmp->state, new_tmp->state);
                    tmp->size = new_tmp->size;
                    tmp->rss = new_tmp->rss;
                    tmp->prio = new_tmp->prio;

                    refresh_list_item(i);
                }
                tmp->checked = TRUE;
                new_tmp->checked = TRUE;
                break;
            }
            else
                tmp->checked = FALSE;
        }
    }

    /* check for unchecked old-tasks for deleting */
    i = 0;
    while( i < task_array->len)
    {

        struct task *tmp = &g_array_index(task_array, struct task, i);

        if(!tmp->checked)
        {
            remove_list_item((gint)tmp->pid);
            g_array_remove_index(task_array, i);
            tasks--;
        }
        else
            i++;

    }


    /* check for unchecked new tasks for inserting */
    for(i = 0; i < new_task_list->len; i++)
    {
        struct task *new_tmp = &g_array_index(new_task_list, struct task, i);

        if(!new_tmp->checked)
        {
            struct task *new_task = new_tmp;

            g_array_append_val(task_array, *new_task);
            if((show_user_tasks && new_task->uid == own_uid) || (show_root_tasks && new_task->uid == 0) ||  (show_other_tasks && new_task->uid != own_uid && new_task->uid != 0))
                add_new_list_item(tasks);
            tasks++;
        }
    }

    g_array_free(new_task_list, TRUE);

    /* update the CPU and memory progress bars */
    if (sys_stat == NULL)
        sys_stat = g_new (system_status, 1);
    get_system_status (sys_stat);

    memory_used = sys_stat->mem_total - sys_stat->mem_free;
    if ( show_cached_as_free )
    {
        memory_used-=sys_stat->mem_cached;
    }
    mem_tooltip = g_strdup_printf (_("%d kB of %d kB used"), memory_used / 1024, sys_stat->mem_total / 1024);
    gtk_tooltips_set_tip (tooltips, mem_usage_progress_bar_box, mem_tooltip, NULL);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (mem_usage_progress_bar),  (gdouble)memory_used / sys_stat->mem_total);

    cpu_usage = get_cpu_usage (sys_stat);
    cpu_tooltip = g_strdup_printf (_("%0.0f %%"), cpu_usage * 100.0);
    gtk_tooltips_set_tip (tooltips, cpu_usage_progress_bar_box, cpu_tooltip, NULL);
    gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (cpu_usage_progress_bar), cpu_usage);

    g_free (mem_tooltip);
    g_free (cpu_tooltip);

    return TRUE;
}

gdouble get_cpu_usage(system_status *sys_stat)
{
    gdouble cpu_usage = 0.0;
    guint current_jiffies;
    guint current_used;
    guint delta_jiffies;

    if ( get_cpu_usage_from_proc( sys_stat ) == FALSE )
    {
        gint i = 0;

        for(i = 0; i < task_array->len; i++)
        {
            struct task *tmp = &g_array_index(task_array, struct task, i);
            cpu_usage += tmp->time_percentage;
        }

        cpu_usage = cpu_usage / (sys_stat->cpu_count * 100.0);
    } else {

        if ( sys_stat->cpu_old_jiffies > 0 ) {
            current_used =
                sys_stat->cpu_user +
                sys_stat->cpu_nice +
                sys_stat->cpu_system;
            current_jiffies =
                current_used +
                sys_stat->cpu_idle;
            delta_jiffies =
                current_jiffies - (gdouble)sys_stat->cpu_old_jiffies;

            cpu_usage = (gdouble)( current_used - sys_stat->cpu_old_used ) /
                     (gdouble)delta_jiffies ;
        }
    }
    return cpu_usage;
}

static gboolean key_file_get_int( GKeyFile* kf, const char* group, const char* name, gboolean def )
{
    int ret;
    GError* err = NULL;
    ret = g_key_file_get_integer( kf, group, name, &err );
    if( err )
    {
        ret = def;
        g_error_free( err );
    }
    return ret;
}

static gboolean key_file_get_bool( GKeyFile* kf, const char* group, const char* name, gboolean def )
{
    return !!key_file_get_int(kf, group, name, def);
}

/*
 * configurationfile support
 */
void load_config(void)
{
    static const char group[]="General";
    GKeyFile *rc_file = g_key_file_new();
    g_key_file_load_from_file(rc_file, config_file, 0, NULL);

    show_user_tasks = key_file_get_bool(rc_file, group, "show_user_tasks", TRUE);
    show_root_tasks = key_file_get_bool(rc_file, group, "show_root_tasks", FALSE);
    show_other_tasks = key_file_get_bool(rc_file, group, "show_other_tasks", FALSE);
    show_cached_as_free = key_file_get_bool(rc_file, group, "show_cached_as_free", TRUE);

    full_view = key_file_get_bool(rc_file, group, "full_view", TRUE);

    win_width = key_file_get_int(rc_file, group, "win_width", 500 );
    win_height = key_file_get_int(rc_file, group, "win_height", 400 );

    g_key_file_free(rc_file);
}

void save_config(void)
{
    FILE* rc_file = fopen( config_file, "w" );

    fputs( "[General]\n", rc_file );
    fprintf( rc_file, "show_user_tasks=%d\n", show_user_tasks);
    fprintf( rc_file, "show_root_tasks=%d\n", show_root_tasks);
    fprintf( rc_file, "show_other_tasks=%d\n", show_other_tasks);
    fprintf( rc_file, "show_cached_as_free=%d\n", show_cached_as_free);

    fprintf( rc_file, "full_view=%d\n", full_view);

    gtk_window_get_size(GTK_WINDOW (main_window), (gint *) &win_width, (gint *) &win_height);

    fprintf( rc_file, "win_width=%d\n", win_width);
    fprintf( rc_file, "win_height=%d\n", win_height);

    fclose(rc_file);
}