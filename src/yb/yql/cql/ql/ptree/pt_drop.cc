//--------------------------------------------------------------------------------------------------
// Copyright (c) YugaByte, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
// in compliance with the License.  You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software distributed under the License
// is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express
// or implied.  See the License for the specific language governing permissions and limitations
// under the License.
//
//
// Treenode definitions for DROP statements.
//--------------------------------------------------------------------------------------------------

#include "yb/yql/cql/ql/ptree/pt_drop.h"
#include "yb/yql/cql/ql/ptree/sem_context.h"

DECLARE_bool(use_cassandra_authentication);

namespace yb {
namespace ql {

PTDropStmt::PTDropStmt(MemoryContext *memctx,
                       YBLocation::SharedPtr loc,
                       ObjectType drop_type,
                       PTQualifiedNameListNode::SharedPtr names,
                       bool drop_if_exists)
    : TreeNode(memctx, loc),
      drop_type_(drop_type),
      names_(names),
      drop_if_exists_(drop_if_exists) {
}

PTDropStmt::~PTDropStmt() {
}

CHECKED_STATUS PTDropStmt::Analyze(SemContext *sem_context) {
  if (drop_type_ == OBJECT_ROLE) {
    RETURN_NOT_AUTH_ENABLED(sem_context);
  }

  if (names_->size() > 1) {
    return sem_context->Error(names_, "Only one object name is allowed in a drop statement",
                              ErrorCode::CQL_STATEMENT_INVALID);
  }

  // Processing object name.
  RETURN_NOT_OK(name()->AnalyzeName(sem_context, drop_type()));

  if (FLAGS_use_cassandra_authentication) {
    switch (drop_type()) {
      case OBJECT_INDEX: FALLTHROUGH_INTENDED;
      case OBJECT_TABLE: {
        RETURN_NOT_OK(sem_context->CheckHasTablePermission(loc(), PermissionType::DROP_PERMISSION,
            yb_table_name()));
        break;
      }
      case OBJECT_TYPE:
        RETURN_NOT_OK(sem_context->CheckHasAllKeyspacesPermission(loc(),
            PermissionType::DROP_PERMISSION));
        break;
      case OBJECT_SCHEMA:
        RETURN_NOT_OK(sem_context->CheckHasKeyspacePermission(loc(),
            PermissionType::DROP_PERMISSION, yb_table_name().namespace_name()));
        break;
      case OBJECT_ROLE:
        RETURN_NOT_OK(sem_context->CheckHasRolePermission(loc(), PermissionType::DROP_PERMISSION,
            name()->QLName()));
        if (sem_context->CurrentRoleName() == name()->QLName()) {
          return sem_context->Error(this, "Cannot DROP primary role for current login",
              ErrorCode::INVALID_REQUEST);
        }
        break;
      default:
        return sem_context->Error(this, ErrorCode::FEATURE_NOT_SUPPORTED);
    }
  }

  return Status::OK();
}

void PTDropStmt::PrintSemanticAnalysisResult(SemContext *sem_context) {
  MCString sem_output("\t", sem_context->PTempMem());

  switch (drop_type()) {
    case OBJECT_TABLE: sem_output += "Table "; break;
    case OBJECT_SCHEMA: sem_output += "Keyspace "; break;
    case OBJECT_TYPE: sem_output += "Type "; break;
    case OBJECT_INDEX: sem_output += "Index "; break;
    case OBJECT_ROLE: sem_output += "Role "; break;
    default: sem_output += "UNKNOWN OBJECT ";
  }

  sem_output += name()->last_name();
  sem_output += (drop_if_exists()? " IF EXISTS" : "");
  VLOG(3) << "SEMANTIC ANALYSIS RESULT (" << *loc_ << "):\n" << sem_output;
}

}  // namespace ql
}  // namespace yb
