// Copyright (c) 2012 Hasso-Plattner-Institut fuer Softwaresystemtechnik GmbH. All rights reserved.
#pragma once

#include "helper/types.h"

#include "pred_common.h"

namespace hyrise {
namespace access {

template <typename T>
class EqualsExpression : public SimpleFieldExpression {
 private:
  ValueId lower_bound;
  std::shared_ptr<storage::BaseDictionary<T>> valueIdMap;
  bool value_exists;

 public:
  T value;

  EqualsExpression(size_t _input, field_t _field, T _value) : SimpleFieldExpression(_input, _field), value(_value) {}

  EqualsExpression(size_t _input, field_name_t _field, T _value)
      : SimpleFieldExpression(_input, _field), value(_value) {}


  EqualsExpression(storage::c_atable_ptr_t& _table, field_t _field, T _value)
      : SimpleFieldExpression(_table, _field), value(_value) {}

  virtual void walk(const std::vector<storage::c_atable_ptr_t>& l) {
    SimpleFieldExpression::walk(l);
    valueIdMap = std::dynamic_pointer_cast<storage::BaseDictionary<T>>(table->dictionaryAt(field));

    value_exists =
        (lower_bound.valueId = valueIdMap->findValueIdForValue(value)) != std::numeric_limits<value_id_t>::max();
  }

  virtual std::unique_ptr<AbstractExpression> clone() {
    return std::unique_ptr<EqualsExpression<T>>(new EqualsExpression<T>(table, field, value));
  }

  virtual ~EqualsExpression() {}

  inline virtual bool operator()(size_t row) { return value_exists && table->getValueId(field, row) == lower_bound; }
};
}
}  // namespace hyrise::access
