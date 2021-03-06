/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010-2016 Facebook, Inc. (http://www.facebook.com)     |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/

#ifndef incl_HPHP_ZEND_EXECUTION_STACK
#define incl_HPHP_ZEND_EXECUTION_STACK

#include "hphp/runtime/ext_zend_compat/php-src/Zend/zend_types.h"
#include "hphp/runtime/base/request-local.h"
#include "hphp/runtime/base/request-event-handler.h"
#include "hphp/runtime/base/req-containers.h"

#include <vector>

namespace HPHP {

enum class ZendStackMode {
  HHVM_STACK,
  SIDE_STACK
};

struct ZendStackEntry {
  ZendStackMode mode;
  void* value; // union { ActRec*, uintptr, zval** }
  TYPE_SCAN_CONSERVATIVE_FIELD(value);
};

struct ZendExecutionStack final : RequestEventHandler {
  static zval** getArg(int i);
  static int numArgs();

  static void push(void* z);
  static void* pop();
  static void pushHHVMStack(void* ar);
  static void popHHVMStack();

  // Instance methods
  void requestInit() override {
    clear();
  }
  void requestShutdown() override {
    clear();
  }

private:
  static ZendExecutionStack & getStack();
  void clear() {
    m_stack = req::vector<ZendStackEntry>{};
    if (m_nullArg) {
      m_nullArg->release();
      m_nullArg = nullptr;
    }
  }
  req::vector<ZendStackEntry> m_stack;
  RefData* m_nullArg;
};

}
#endif // incl_HPHP_ZEND_EXECUTION_STACK
