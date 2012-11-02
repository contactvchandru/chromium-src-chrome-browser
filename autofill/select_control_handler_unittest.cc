// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/stringprintf.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/select_control_handler.h"
#include "chrome/common/form_field_data.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(SelectControlHandlerTest, CreditCardMonthExact) {
  const char* const kMonthsNumeric[] = {
    "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12",
  };
  std::vector<string16> options(arraysize(kMonthsNumeric));
  for (size_t i = 0; i < arraysize(kMonthsNumeric); ++i) {
    options[i] = ASCIIToUTF16(kMonthsNumeric[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  CreditCard credit_card;
  credit_card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("01"));
  autofill::FillSelectControl(credit_card, CREDIT_CARD_EXP_MONTH, &field);
  EXPECT_EQ(ASCIIToUTF16("01"), field.value);
}

TEST(SelectControlHandlerTest, CreditCardMonthAbbreviated) {
  const char* const kMonthsAbbreviated[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
  };
  std::vector<string16> options(arraysize(kMonthsAbbreviated));
  for (size_t i = 0; i < arraysize(kMonthsAbbreviated); ++i) {
    options[i] = ASCIIToUTF16(kMonthsAbbreviated[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  CreditCard credit_card;
  credit_card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("01"));
  autofill::FillSelectControl(credit_card, CREDIT_CARD_EXP_MONTH, &field);
  EXPECT_EQ(ASCIIToUTF16("Jan"), field.value);
}

TEST(SelectControlHandlerTest, CreditCardMonthFull) {
  const char* const kMonthsFull[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
  };
  std::vector<string16> options(arraysize(kMonthsFull));
  for (size_t i = 0; i < arraysize(kMonthsFull); ++i) {
    options[i] = ASCIIToUTF16(kMonthsFull[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  CreditCard credit_card;
  credit_card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("01"));
  autofill::FillSelectControl(credit_card, CREDIT_CARD_EXP_MONTH, &field);
  EXPECT_EQ(ASCIIToUTF16("January"), field.value);
}

TEST(SelectControlHandlerTest, CreditCardMonthNumeric) {
  const char* const kMonthsNumeric[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
  };
  std::vector<string16> options(arraysize(kMonthsNumeric));
  for (size_t i = 0; i < arraysize(kMonthsNumeric); ++i) {
    options[i] = ASCIIToUTF16(kMonthsNumeric[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  CreditCard credit_card;
  credit_card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("01"));
  autofill::FillSelectControl(credit_card, CREDIT_CARD_EXP_MONTH, &field);
  EXPECT_EQ(ASCIIToUTF16("1"), field.value);
}

TEST(SelectControlHandlerTest, CreditCardTwoDigitYear) {
  const char* const kYears[] = {
    "12", "13", "14", "15", "16", "17", "18", "19"
  };
  std::vector<string16> options(arraysize(kYears));
  for (size_t i = 0; i < arraysize(kYears); ++i) {
    options[i] = ASCIIToUTF16(kYears[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  CreditCard credit_card;
  credit_card.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("2017"));
  autofill::FillSelectControl(credit_card, CREDIT_CARD_EXP_4_DIGIT_YEAR,
                              &field);
  EXPECT_EQ(ASCIIToUTF16("17"), field.value);
}

TEST(SelectControlHandlerTest, CreditCardType) {
  const char* const kCreditCardTypes[] = {
    "Visa", "Master Card", "AmEx", "discover"
  };
  std::vector<string16> options(arraysize(kCreditCardTypes));
  for (size_t i = 0; i < arraysize(kCreditCardTypes); ++i) {
    options[i] = ASCIIToUTF16(kCreditCardTypes[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  // Credit card types are inferred from the numbers, so we use test numbers for
  // each card type.  Test card numbers are drawn from
  // http://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm

  {
    // Normal case:
    CreditCard credit_card;
    credit_card.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111111111111111"));
    autofill::FillSelectControl(credit_card, CREDIT_CARD_TYPE, &field);
    EXPECT_EQ(ASCIIToUTF16("Visa"), field.value);
  }

  {
    // Filling should be able to handle intervening whitespace:
    CreditCard credit_card;
    credit_card.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("5105105105105100"));
    autofill::FillSelectControl(credit_card, CREDIT_CARD_TYPE, &field);
    EXPECT_EQ(ASCIIToUTF16("Master Card"), field.value);
  }

  {
    // American Express is sometimes abbreviated as AmEx:
    CreditCard credit_card;
    credit_card.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("371449635398431"));
    autofill::FillSelectControl(credit_card, CREDIT_CARD_TYPE, &field);
    EXPECT_EQ(ASCIIToUTF16("AmEx"), field.value);
  }

  {
    // Case insensitivity:
    CreditCard credit_card;
    credit_card.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("6011111111111117"));
    autofill::FillSelectControl(credit_card, CREDIT_CARD_TYPE, &field);
    EXPECT_EQ(ASCIIToUTF16("discover"), field.value);
  }
}

TEST(SelectControlHandlerTest, AddressCountryFull) {
  const char* const kCountries[] = {
    "Albania", "Canada"
  };
  std::vector<string16> options(arraysize(kCountries));
  for (size_t i = 0; i < arraysize(kCountries); ++i) {
    options[i] = ASCIIToUTF16(kCountries[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  AutofillProfile profile;
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("CA"));
  autofill::FillSelectControl(profile, ADDRESS_HOME_COUNTRY, &field);
  EXPECT_EQ(ASCIIToUTF16("Canada"), field.value);
}

TEST(SelectControlHandlerTest, AddressCountryAbbrev) {
  const char* const kCountries[] = {
    "AL", "CA"
  };
  std::vector<string16> options(arraysize(kCountries));
  for (size_t i = 0; i < arraysize(kCountries); ++i) {
    options[i] = ASCIIToUTF16(kCountries[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  AutofillProfile profile;
  profile.SetInfo(ADDRESS_HOME_COUNTRY, ASCIIToUTF16("Canada"));
  autofill::FillSelectControl(profile, ADDRESS_HOME_COUNTRY, &field);
  EXPECT_EQ(ASCIIToUTF16("CA"), field.value);
}

TEST(SelectControlHandlerTest, AddressStateFull) {
  const char* const kStates[] = {
    "Alabama", "California"
  };
  std::vector<string16> options(arraysize(kStates));
  for (size_t i = 0; i < arraysize(kStates); ++i) {
    options[i] = ASCIIToUTF16(kStates[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  AutofillProfile profile;
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("CA"));
  autofill::FillSelectControl(profile, ADDRESS_HOME_STATE, &field);
  EXPECT_EQ(ASCIIToUTF16("California"), field.value);
}

TEST(SelectControlHandlerTest, AddressStateAbbrev) {
  const char* const kStates[] = {
    "AL", "CA"
  };
  std::vector<string16> options(arraysize(kStates));
  for (size_t i = 0; i < arraysize(kStates); ++i) {
    options[i] = ASCIIToUTF16(kStates[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = options;
  field.option_contents = options;

  AutofillProfile profile;
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"));
  autofill::FillSelectControl(profile, ADDRESS_HOME_STATE, &field);
  EXPECT_EQ(ASCIIToUTF16("CA"), field.value);
}

TEST(SelectControlHandlerTest, FillByValue) {
  const char* const kStates[] = {
    "Alabama", "California"
  };
  std::vector<string16> values(arraysize(kStates));
  std::vector<string16> contents(arraysize(kStates));
  for (size_t i = 0; i < arraysize(kStates); ++i) {
    values[i] = ASCIIToUTF16(kStates[i]);
    contents[i] = ASCIIToUTF16(base::StringPrintf("%d", static_cast<int>(i)));
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = values;
  field.option_contents = contents;

  AutofillProfile profile;
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"));
  autofill::FillSelectControl(profile, ADDRESS_HOME_STATE, &field);
  EXPECT_EQ(ASCIIToUTF16("California"), field.value);
}

TEST(SelectControlHandlerTest, FillByContents) {
  const char* const kStates[] = {
    "Alabama", "California"
  };
  std::vector<string16> values(arraysize(kStates));
  std::vector<string16> contents(arraysize(kStates));
  for (size_t i = 0; i < arraysize(kStates); ++i) {
    values[i] = ASCIIToUTF16(base::StringPrintf("%d", static_cast<int>(i + 1)));
    contents[i] = ASCIIToUTF16(kStates[i]);
  }

  FormFieldData field;
  field.form_control_type = "select-one";
  field.option_values = values;
  field.option_contents = contents;

  AutofillProfile profile;
  profile.SetInfo(ADDRESS_HOME_STATE, ASCIIToUTF16("California"));
  autofill::FillSelectControl(profile, ADDRESS_HOME_STATE, &field);
  EXPECT_EQ(ASCIIToUTF16("2"), field.value);
}
