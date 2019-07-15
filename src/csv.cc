/*
 * Copyright (c) 2003-2018, John Wiegley.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * - Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * - Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *   documentation and/or other materials provided with the distribution.
 *
 * - Neither the name of New Artisans LLC nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <system.hh>

#include "csv.h"
#include "xact.h"
#include "post.h"
#include "account.h"
#include "journal.h"
#include "pool.h"

namespace ledger {

string csv_reader::read_field(std::istream& in)
{
  string field;

  char c;
  if (in.peek() == '"' || in.peek() == '|') {
    in.get(c);
    char x;
    while (in.good() && ! in.eof()) {
      in.get(x);
      if (x == '\\') {
        in.get(x);
      }
      else if (x == '"' && in.peek() == '"') {
        in.get(x);
      }
      else if (x == c) {
        if (x == '|')
          in.unget();
        else if (in.peek() == ',')
          in.get(c);
        break;
      }
      if (x != '\0')
        field += x;
    }
  }
  else {
    while (in.good() && ! in.eof()) {
      in.get(c);
      if (in.good()) {
        if (c == ',')
          break;
        if (c != '\0')
          field += c;
      }
    }
  }
  trim(field);
  return field;
}

char * csv_reader::next_line(std::istream& in)
{
  while (in.good() && ! in.eof() && in.peek() == '#')
    in.getline(context.linebuf, parse_context_t::MAX_LINE);

  if (! in.good() || in.eof() || in.peek() == -1)
    return NULL;

  in.getline(context.linebuf, parse_context_t::MAX_LINE);

  return context.linebuf;
}

void csv_reader::read_index(std::istream& in)
{
  char * line = next_line(in);
  if (! line)
    return;

  std::istringstream instr(line);

  while (instr.good() && ! instr.eof()) {
    string field = read_field(instr);
    names.push_back(field);

    if (date_mask.match(field))
      index.push_back(FIELD_DATE);
    else if (date_aux_mask.match(field))
      index.push_back(FIELD_DATE_AUX);
    else if (code_mask.match(field))
      index.push_back(FIELD_CODE);
    else if (payee_mask.match(field))
      index.push_back(FIELD_PAYEE);
    else if (ameritrade_payee_mask.match(field))
      index.push_back(FIELD_AMERITRADE_PAYEE);
    else if (amount_mask.match(field))
      index.push_back(FIELD_AMOUNT);
    else if (cost_mask.match(field))
      index.push_back(FIELD_COST);
    else if (total_mask.match(field))
      index.push_back(FIELD_TOTAL);
    else if (note_mask.match(field))
      index.push_back(FIELD_NOTE);
    else
      index.push_back(FIELD_UNKNOWN);

    DEBUG("csv.parse", "Header field: " << field);
  }
}

xact_t * csv_reader::read_xact(bool rich_data)
{
  char * line = next_line(*context.stream.get());
  if (! line || index.empty())
    return NULL;
  context.linenum++;

  std::istringstream instr(line);

  unique_ptr<xact_t> xact(new xact_t);
  unique_ptr<post_t> post(new post_t);

  xact->set_state(item_t::CLEARED);

  xact->pos           = position_t();
  xact->pos->pathname = context.pathname;
  xact->pos->beg_pos  = context.stream->tellg();
  xact->pos->beg_line = context.linenum;
  xact->pos->sequence = context.sequence++;

  post->xact = xact.get();

  post->pos           = position_t();
  post->pos->pathname = context.pathname;
  post->pos->beg_pos  = context.stream->tellg();
  post->pos->beg_line = context.linenum;
  post->pos->sequence = context.sequence++;

  post->set_state(item_t::CLEARED);
  post->account = NULL;

  std::vector<int>::size_type n = 0;
  amount_t amt;
  amount_t adjust;
  amount_t cost;
  string total;
  string field;
  bool saw_ameritrade = true;

#if HAVE_BOOST_REGEX_UNICODE
  boost::u32regex
#else
  boost::regex
#endif
    ameritrade("(?:(?:Ctrl )?(?:Shift [BSF] )?|tIP )(BOT|SOLD) ([-+])([0-9]+) (([A-Z]+)(?: ([0-9]+).+?)?) @([-0-9.]+)( [A-Z]+)?");

  while (instr.good() && ! instr.eof() && n < index.size()) {
    field = read_field(instr);

    switch (index[n]) {
    case FIELD_DATE:
      xact->_date = parse_date(field);
      break;

    case FIELD_DATE_AUX:
      if (! field.empty())
        xact->_date_aux = parse_date(field);
      break;

    case FIELD_CODE:
      if (! field.empty())
        xact->code = field;
      break;

    case FIELD_PAYEE: {
      bool found = false;
      foreach (payee_alias_mapping_t& value, context.journal->payee_alias_mappings) {
        DEBUG("csv.mappings", "Looking for payee mapping: " << value.first);
        if (value.first.match(field)) {
          xact->payee = value.second;
          found = true;
          break;
        }
      }
      if (! found)
        xact->payee = field;
      break;
    }

    case FIELD_AMERITRADE_PAYEE: {
      boost::match_results<std::string::const_iterator> what;

      if (
#if HAVE_BOOST_REGEX_UNICODE
        boost::u32regex_search(field, what, ameritrade)
#else
        boost::regex_search(field, what, ameritrade)
#endif
        ) {
        std::string bot_or_sold(what[1]);
        std::string direction(what[2]);
        std::string amount(what[3]);
        std::string full_ticker(what[4]);
        std::string symbol(what[5]);
        std::string size(what[6]);
        std::string cost(what[7]);
        std::string exchange(what[8]);

        post->account = context.master;

        if (direction == "+") {
          direction = "";
        }

        if (full_ticker.find(' ') != std::string::npos) {
          full_ticker = std::string("\"") + full_ticker + "\"";
        }

        amount_t amount_amt;
        std::istringstream amount_str(direction + amount + " " + full_ticker);
        amount_amt.parse(amount_str, PARSE_NO_REDUCE);
        if (! amount_amt.has_commodity() &&
            commodity_pool_t::current_pool->default_commodity)
          amount_amt.set_commodity(*commodity_pool_t::current_pool->default_commodity);

        post->amount = amount_amt;

        amount_t cost_amt;
        std::istringstream cost_str(cost);
        cost_amt.parse(cost_str, PARSE_NO_REDUCE);
        if (! cost_amt.has_commodity() &&
            commodity_pool_t::current_pool->default_commodity)
          cost_amt.set_commodity(*commodity_pool_t::current_pool->default_commodity);

        cost_amt *= amount_amt;
        if (! size.empty())
          cost_amt *= amount_t(size);
        post->cost = cost_amt;
        post->cost->in_place_unround();
        post->given_cost = post->cost;

        saw_ameritrade = true;
      } else {
        saw_ameritrade = false;
      }

      xact->payee = field;
      break;
    }

    case FIELD_AMOUNT: {
      std::istringstream amount_str(field);
      amt.parse(amount_str, PARSE_NO_REDUCE);
      if (! amt.has_commodity() &&
          commodity_pool_t::current_pool->default_commodity)
        amt.set_commodity(*commodity_pool_t::current_pool->default_commodity);
      if (! saw_ameritrade) {
        post->amount = amt;
      } else {
        if (! adjust.is_null())
          amt += adjust;
        amt.in_place_negate();

        annotation_t details(amt);
        // details.add_flags(ANNOTATION_PRICE_FIXATED);
        post->amount.annotate(details);
      }
      break;
    }

    case FIELD_COST: {
      std::istringstream amount_str(field);
      cost.parse(amount_str, PARSE_NO_REDUCE);
      if (! cost.has_commodity() &&
          commodity_pool_t::current_pool->default_commodity)
        cost.set_commodity
          (*commodity_pool_t::current_pool->default_commodity);
      post->cost = cost;
      break;
    }

    case FIELD_TOTAL:
      total = field;
      break;

    case FIELD_NOTE:
      if (! field.empty())
        xact->note = field;
      break;

    case FIELD_UNKNOWN:
      if (! names[n].empty() && ! field.empty()) {
        if (saw_ameritrade && names[n] == "Fees") {
          unique_ptr<post_t> post(new post_t);

          post->xact = xact.get();

          post->pos           = position_t();
          post->pos->pathname = context.pathname;
          post->pos->beg_pos  = context.stream->tellg();
          post->pos->beg_line = context.linenum;
          post->pos->sequence = context.sequence++;

          post->set_state(item_t::CLEARED);
          post->account = context.journal->find_account(_("Expenses:TD:Fees"));
          post->add_flags(POST_VIRTUAL);

          amount_t famt(std::string("$") + field);
          post->amount = famt.negated();
          if (adjust.is_null())
            adjust = famt;
          else
            adjust += famt;
          xact->add_post(post.release());
        }
        else if (saw_ameritrade && names[n] == "Commission") {
          unique_ptr<post_t> post(new post_t);

          post->xact = xact.get();

          post->pos           = position_t();
          post->pos->pathname = context.pathname;
          post->pos->beg_pos  = context.stream->tellg();
          post->pos->beg_line = context.linenum;
          post->pos->sequence = context.sequence++;

          post->set_state(item_t::CLEARED);
          post->account = context.journal->find_account(_("Expenses:TD:Commission"));
          post->add_flags(POST_VIRTUAL);

          amount_t famt(std::string("$") + field);
          post->amount = famt.negated();
          if (adjust.is_null())
            adjust = famt;
          else
            adjust += famt;
          xact->add_post(post.release());
        }
        else {
          xact->set_tag(names[n], string_value(field));
        }
      }
      break;
    }
    n++;
  }

  if (rich_data) {
    xact->set_tag(_("Imported"),
                  string_value(format_date(CURRENT_DATE(), FMT_WRITTEN)));
    xact->set_tag(_("CSV"), string_value(line));
  }

  // Translate the account name, if we have enough information to do so

  foreach (account_mapping_t& value, context.journal->payees_for_unknown_accounts) {
    if (value.first.match(xact->payee)) {
      post->account = value.second;
      break;
    }
  }

  xact->add_post(post.release());

  // Create the "balancing post", which refers to the account for this data

  post.reset(new post_t);

  post->xact = xact.get();

  post->pos           = position_t();
  post->pos->pathname = context.pathname;
  post->pos->beg_pos  = context.stream->tellg();
  post->pos->beg_line = context.linenum;
  post->pos->sequence = context.sequence++;

  post->set_state(item_t::CLEARED);
  post->account = context.master;

  if (! amt.is_null())
    post->amount = amt.negated();

  if (! total.empty()) {
    std::istringstream assigned_amount_str(total);
    amt.parse(assigned_amount_str, PARSE_NO_REDUCE);
    if (! amt.has_commodity() &&
        commodity_pool_t::current_pool->default_commodity)
      amt.set_commodity(*commodity_pool_t::current_pool->default_commodity);
    post->assigned_amount = amt;
  }

  xact->add_post(post.release());

  unique_ptr<post_t> bpost(new post_t);

  bpost->xact = xact.get();

  bpost->pos           = position_t();
  bpost->pos->pathname = context.pathname;
  bpost->pos->beg_pos  = context.stream->tellg();
  bpost->pos->beg_line = context.linenum;
  bpost->pos->sequence = context.sequence++;

  bpost->set_state(item_t::CLEARED);
  bpost->account = context.journal->find_account(_("Income:Capital:Short"));
  // bpost->amount = amount_t(0L);
  xact->add_post(bpost.release());

  return xact.release();
}

} // namespace ledger
