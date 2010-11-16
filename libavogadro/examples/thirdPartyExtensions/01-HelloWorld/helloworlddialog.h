/**********************************************************************
  HelloWorldExtension

  Copyright (C) 2010 David C. Lonie

  This library is free software; you can redistribute it and/or modify
  it under the terms of the GNU Library General Public License as
  published by the Free Software Foundation; either version 2.1 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public icense for more details.
 ***********************************************************************/

#ifndef HELLOWORLDDIALOG_H
#define HELLOWORLDDIALOG_H

#include <QtGui/QDialog>

// Include autogenerated ui code
#include "ui_helloworlddialog.h"

// Forward declaration of Avogadro::Molecule
namespace Avogadro {
  class Molecule;
}

namespace HelloWorld {

  class HelloWorldDialog : public QDialog
  {
    // The Q_OBJECT macro must be include if a class is to have
    // signals/slots
    Q_OBJECT

  public:
    explicit HelloWorldDialog(QWidget *parent = 0, Qt::WindowFlags f = 0 );
    virtual ~HelloWorldDialog() {};

  private:
    // This member provides access to all ui elements
    Ui::HelloWorldDialog ui;
  };
}

#endif