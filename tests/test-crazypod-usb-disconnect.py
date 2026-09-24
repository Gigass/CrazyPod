#!/usr/bin/env python3
"""Exercise physical USB removal before enumeration and during storage."""
from pathlib import Path
import re
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
shell = root / 'apps/crazypod/ui/shell'

def function(file, name):
    source = (shell / file).read_text()
    match = re.search(r'(?:static )?(?:void|bool)\s+' + name +
                      r'\([^)]*\)\s*\{.*?^\}', source, re.S | re.M)
    assert match, name
    return match.group()

code = r'''
#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "apps/crazypod/ui/shell/crazypod_system_event.h"
#define HAVE_USB_POWER
#define SYS_EVENT 0x100
#define CRAZYPOD_USB_PROMPT_REQUEST 0x131
#define CRAZYPOD_USB_PROMPT_DONE 0x132
#define CRAZYPOD_USB_PROMPT_EXTRACTED 0x133
#define SYS_USB_CONNECTED 0x101
#define SYS_USB_DISCONNECTED 0x102
#define SYS_POWEROFF 0x103
#define SYS_REBOOT 0x104
static struct { bool storage_active; } prompts;
static struct { unsigned request_id; } prompt;
static unsigned visible_request;
static bool visible;
static long queued_event;
static intptr_t queued_data;
static void button_queue_post(long event, intptr_t data)
{ queued_event = event; queued_data = data; }
static bool crazypod_usb_prompt_matches_request(unsigned request)
{ return visible && request == visible_request; }
static void crazypod_usb_prompt_dismiss(void) { visible = false; }
'''
code += function('crazypod_usb_prompt.c', 'extracted_event')
code += function('crazypod_system_prompts.c', 'crazypod_system_prompts_usb_extracted')
code += function('crazypod_system_event.c', 'crazypod_system_event_handle')
code += r'''
int main(void)
{
    struct crazypod_system_event_actions actions = {
        .usb_prompt_extracted = crazypod_system_prompts_usb_extracted,
    };
    visible = true;
    visible_request = prompt.request_id = 7;
    extracted_event(0, NULL);
    assert(visible); /* Worker callback must not mutate the UI. */
    assert(crazypod_system_event_handle(queued_event, queued_data, &actions));
    assert(!visible); /* No mass-storage session: dismiss on physical removal. */
    visible = true;
    prompts.storage_active = true;
    extracted_event(0, NULL);
    assert(crazypod_system_event_handle(queued_event, queued_data, &actions));
    assert(visible && prompts.storage_active); /* Wait for remount. */
    prompts.storage_active = false;
    visible_request = 8;
    assert(crazypod_system_event_handle(queued_event, queued_data, &actions));
    assert(visible); /* An old removal cannot dismiss a new connection. */
    prompt.request_id = 8;
    extracted_event(0, NULL);
    assert(crazypod_system_event_handle(queued_event, queued_data, &actions));
    assert(!visible);
    return 0;
}
'''
with tempfile.TemporaryDirectory() as directory:
    path = Path(directory)
    (path / 'test.c').write_text(code)
    subprocess.run(['cc', '-std=c99', '-Wall', '-Wextra', '-Werror',
                    '-I', str(root), str(path / 'test.c'), '-o',
                    str(path / 'test')], check=True)
    subprocess.run([str(path / 'test')], check=True)
print('USB removal: unenumerated, storage-active and stale-request cases passed')
