# DEB2 TODOS

## INTRODUCTION

DEB2 is a set of conventions that sit on top of Ledger Journal syntax
that set the stage for updated Double Entry Bookkeeping Features
for the future.

## EXAMPLE

~~~~~~~~~~~~~~
2015-10-12 (product-transfer) GALGAS transferred from GASSTATION to CAR  ; location:302PalmerAve
  ; timestamp:2015-10-12 11:34:23.232
  ; eventID:20151012-113423-01231
  ; xeventID:E20151012-113834-01293
  ANDY:R:Vehicles:HondaFit:Gastank:GALGAS 4.523 GALGAS  ; payee:MOBIL
  ANDY:I:Vehicles:HondaFit:GasInlet_In      -4.523 GALGAS  ; payee:MOBIL
  MOBIL:O:Equip:GasPump04:Nozzle_Out          4.523 GALGAS  ; payee:ANDY
  MOBIL:R:Inventory:StorageTank:GALGAS   -4.523 GALGAS  ; payee:ANDY

2015-10-13 (product-payment) USD transferred from ANDY to GASSTATION  ; location:302PalmerAve
  ; timestamp:2015-10-12 11:38:34.645
  ; eventID:E20151012-113834-01293
  ; xeventID:E20151012-113423-01231
  MOBIL:R:Assets:CashRegister02:USD       10.00 USD     ; payee:ANDY
  MOBIL:I:Income:GasSales_In             -10.00 USD     ; payee:ANDY
  ANDY:O:Expenses:Vehicles:Fuel_Out           10.00 USD     ; payee:MOBIL
  ANDY:R:Assets:Wallet:Cash:USD          -10.00 USD     ; payee:MOBIL
~~~~~~~~~~~~~

## LEDGER-COMPLIANT JOURNAL SYNTAX CONVENTIONS FOR DEB2

No changes are necessary to support these DEB2 conventions

### Journal file scope
1. *Special Metadata terms* - add comments to specify journal-file level metadata for future
  * registry used for entity names
  * registry used for account names
  * registry used for commodities
  * registry used for action types

### Transaction scope
1. *Actiontype in paren* - convention is that action names always be placed in parens
1. *Special transaction scope metadata*
  * timestamp:YYYY-MM-DDTHH:MM:SS.FFFFF - timestamp of when action began
  * eventUID:<UID> - unique identifier for this action
  * xeventID:<UID> - unique identifier for an action seen as 2nd part of an exchange for this action
### Single post scope

1. *Uppercase at start of accountname* - is name of subject entity (determiner of names in accountname). Subject
   also known as Point of View (POV).
1. *Uppercase at end of accountname* - commodity type, these are only used for R and C account types
1. *_In and _Out* - used to more fully identify that account is incoming or outgoing
1. _

## FUTURE JOURNAL SYNTAX ENHANCEMENTS FOR DEB2
Changes necessary to support these DEB2 conventions

### Journal file scope
1. *Journal File Scope Metadata* - add ability to include key metadata about a journal file
  * Closed:<Date> indicates the date that the journal was made read-only
  * VocabUID:<Handle> indicates a vocabulary registry w single name or hierarchy of names you plan to use for accounts

### Transaction scope
1. *Full ISO Date and Time* - Abilty to use full ISO dates on lines 1 & 10 (eg. 2015-10-12T12:00:01.24324)
1. *Location Metadata* - Add new special metadata keyword (and semantics)for indicating location of action

### Single post scope

1. *Object as synonymn for Payee* - [DONE]

## NEW FEATURES FOR LEDGER-CLI

1. *ISO Time processing in value expressions* - add ability to perform ISO datateime functions in value expresssions
1. *Description Report* - add ability to print out a Description report by time (noting description line at transaction
   scope is assigned as Payee but we choose to use payee to indicate object.


