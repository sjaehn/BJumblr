/*
  Copyright 2012-2020 David Robillard <d@drobilla.net>

  Permission to use, copy, modify, and/or distribute this software for any
  purpose with or without fee is hereby granted, provided that the above
  copyright notice and this permission notice appear in all copies.

  THIS SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/

#include "stub.h"
#include "types.h"
#include "win.h"

#include "pugl/stub.h"

PuglStatus
puglWinStubConfigure(PuglView* view)
{
  PuglInternals* const impl = view->impl;
  PuglStatus           st   = PUGL_SUCCESS;

  if ((st = puglWinCreateWindow(view, "Pugl", &impl->hwnd, &impl->hdc))) {
    return st;
  }

  impl->pfd  = puglWinGetPixelFormatDescriptor(view->hints);
  impl->pfId = ChoosePixelFormat(impl->hdc, &impl->pfd);

  if (!SetPixelFormat(impl->hdc, impl->pfId, &impl->pfd)) {
    ReleaseDC(impl->hwnd, impl->hdc);
    DestroyWindow(impl->hwnd);
    impl->hwnd = NULL;
    impl->hdc  = NULL;
    return PUGL_SET_FORMAT_FAILED;
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglWinStubEnter(PuglView* view, const PuglEventExpose* expose)
{
  if (expose) {
    PAINTSTRUCT ps;
    BeginPaint(view->impl->hwnd, &ps);
  }

  return PUGL_SUCCESS;
}

PuglStatus
puglWinStubLeave(PuglView* view, const PuglEventExpose* expose)
{
  if (expose) {
    PAINTSTRUCT ps;
    EndPaint(view->impl->hwnd, &ps);
  }

  return PUGL_SUCCESS;
}

const PuglBackend*
puglStubBackend(void)
{
  static const PuglBackend backend = {puglWinStubConfigure,
                                      puglStubCreate,
                                      puglStubDestroy,
                                      puglWinStubEnter,
                                      puglWinStubLeave,
                                      puglStubGetContext};

  return &backend;
}
