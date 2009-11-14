/**********************************************************************
  propextension.h - Properties Plugin for Avogadro

  Copyright (C) 2009 by Konstantin Tokarev
  Based on code written by Tim Vandermeersch and Geoffrey R. Hutchison

  This file is part of the Avogadro molecular editor project.
  For more information, see <http://avogadro.openmolecules.net/>

  Some code is based on Open Babel
  For more information, see <http://openbabel.sourceforge.net/>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
 ***********************************************************************/

#include "cartesianextension.h"

#include <avogadro/glwidget.h>
#include <avogadro/molecule.h>
#include <avogadro/atom.h>
#include <avogadro/bond.h>
#include <avogadro/primitivelist.h>

#include <QtGui/QAction>
#include <QtGui/QDialog>
#include <QtGui/QClipboard>

#include <QtCore/QDebug>
#include <QtCore/QStringList>
#include <QtCore/QTextStream>

using namespace std;
using namespace OpenBabel;

namespace Avogadro
{
  static const double BOHR_TO_ANGSTROM = 0.529177249;
  static const double ANGSTROM_TO_BOHR = 1.0 / 0.529177249;

  int GetAtomicNum(string name, int &iso);
  
  CartesianEditor::CartesianEditor(QWidget *parent) : QDialog(parent),
                           m_unit(0), m_format(0), m_illegalInput(false)
  {
    setupUi(this);
    cartesianEdit->setTextColor(Qt::black);
	cartesianEdit->setFontPointSize(QApplication::font().pointSize()+1);

    connect(unitsBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeUnits()));
    connect(formatBox, SIGNAL(currentIndexChanged(int)), this, SLOT(changeFormat()));
    
    connect(pasteButton, SIGNAL(clicked()), this, SLOT(paste()));
    connect(copyButton, SIGNAL(clicked()), this, SLOT(copy()));
    connect(cutButton, SIGNAL(clicked()), this, SLOT(cut()));
    connect(clearButton, SIGNAL(clicked()), this, SLOT(clear()));

    connect(cartesianEdit, SIGNAL(textChanged()), this, SLOT(textChanged()));

    connect(applyButton, SIGNAL(clicked()), this, SLOT(updateMolecule()));
    connect(revertButton, SIGNAL(clicked()), this, SLOT(updateCoordinates()));
  }

  void CartesianEditor::changeUnits()
  {
    m_unit = unitsBox->currentIndex();
    updateCoordinates();
  }

  void CartesianEditor::changeFormat()
  {
    m_format = formatBox->currentIndex();
    updateCoordinates();
  }
    
  void CartesianEditor::paste()
  {
    QClipboard *clipboard = QApplication::clipboard();
    cartesianEdit->append(clipboard->text());
  }

  void CartesianEditor::copy()
  {
    QClipboard *clipboard = QApplication::clipboard();
    clipboard->setText(cartesianEdit->toPlainText());
  }

  void CartesianEditor::cut()
  {
    copy();
    clear();
  }

  void CartesianEditor::clear()
  {
    cartesianEdit->setText("");
  }

  void CartesianEditor::textChanged()
  {
    if (m_illegalInput) {
      m_illegalInput = false;
      cartesianEdit->setTextColor(Qt::black);
      QString t = cartesianEdit->toPlainText();
      cartesianEdit->setText(t);
    }
  }

  void CartesianEditor::updateMolecule()
  {
    OBMol *tmpMol = new OBMol;
    if (parseText(tmpMol)) {
      m_molecule->setOBMol(tmpMol);
      m_molecule->update();
      updateCoordinates();
    } else {
      cartesianEdit->setTextColor(Qt::red);
      QString t = cartesianEdit->toPlainText();
      cartesianEdit->setText(t);
      m_illegalInput = true;
    }
    delete tmpMol;
  }
  
  bool CartesianEditor::parseText(OBMol *mol)
  {
    QString coord = cartesianEdit->toPlainText();
    QStringList coordStrings = coord.split(QRegExp("\n"));
    
    double k;
    switch (m_unit) {
    case ANGSTROM:
      k=1.0;
      break;
    case BOHR:
      k=BOHR_TO_ANGSTROM;
      break;
    }
    
    // Guess format

    // split on any non-word symbol, except '.'
    QStringList data = coordStrings.at(0).trimmed().split(QRegExp("\\s+|,|;")); 
    // Format definition, will be used for parsing
    int NameCol=-1, Xcol=-1, Ycol=-1, Zcol=-1;
    QString format("");
    int a;
    double b;
    bool ok;
    for (int i=0; i<data.size(); i++) {
      if (data.at(i) == "") {
        continue;
      }
      
      a = data.at(i).toInt(&ok);
      if (ok) {
        format += "i";
        continue;
      }
      
      b = data.at(i).toDouble(&ok);
      if (ok) {
        if ((int)b == b && b!=0)
          format += "i";    // non-zero integer found - not likely to be coordinate
        else
          format += "d";
      } else {
        format += "s";
      }
    }

    qDebug() << "Format is: " << format;

    if (format.length() < 4)
      return false; // invalid format

    if (format == "iddd") { // special XYZ variant
      NameCol=0;
      Xcol=1;
      Ycol=2;
      Zcol=3;
    }
    else { // more columns
        for (int i=0; i<format.length(); i++) {
            //if (format.at(i) == 'i')
              //continue; // nothing valuable
              
            if ((format.at(i)=='d') || (format.length()==4 && format.at(i)=='i')) {
              // double
              if (Xcol==-1) {
                Xcol=i;
                continue;
              }
              if (Ycol==-1) {
                Ycol=i;
                continue;
              }
              if (Zcol==-1) {
                Zcol=i;
                continue;
              }
              continue; // nothing valuable
            }

            if (format.at(i) == 's') {
              // string
              if (NameCol != -1)  // just found
                continue;
          
              // Try to find element name or symbol inside it
              int n,iso;
              QString s = data.at(i);
              while (s.length()!=0) { // recognize name with number
                iso = 0;
                n = GetAtomicNum(s.toStdString(), iso);
                if (iso != 0)
                  n = 1;
            
                if (n!=0) {
                  NameCol=i;
                  break;
                } else {
                  s.chop(1);
                }
              }
            }
            continue;
        }
    }

    if((NameCol==-1) || (Xcol==-1) || (Ycol==-1) || (Zcol==-1)) {
      return false;
    }
      
    // Read and apply coordinates
    mol->BeginModify();
    for (int N=0; N<coordStrings.size(); N++) {
      if (coordStrings.at(N) == "") {
        continue;
      }
      
      OBAtom *atom  = mol->NewAtom();
      QStringList s_data = coordStrings.at(N).trimmed().split(QRegExp("\\s+|,|;"));
      if (s_data.size() != data.size()) {
        return false;
      }
      for (int i=0; i<s_data.size(); i++) {
        double x, y, z;
        int _n,_iso;
        bool ok = true;
        if (i == Xcol) {
            x = s_data.at(i).toDouble(&ok);
        } else if (i == Ycol) {
            y = s_data.at(i).toDouble(&ok);
        } else if (i == Zcol) {
            z = s_data.at(i).toDouble(&ok);
        } else if ((i == NameCol) && (format == "iddd")) {
            _n = s_data.at(i).toInt(&ok);
        } else if (i == NameCol) {

            // Try to find element name or symbol inside it
              
              QString _s = s_data.at(i);
              while (_s.length()!=0) { // recognize name with number
                _iso=0;  
                _n = GetAtomicNum(_s.toStdString(), _iso);
                if (_iso != 0)
                  _n = 1;
            
                if (_n!=0)
                  break;
                else
                  _s.chop(1);
              }
              if (_n==0)
                return false;
        }
        if (!ok) return false;
        
        atom->SetAtomicNum(_n);
        atom->SetVector(x*k,y*k,z*k); //set coordinates
      }
    }
    mol->EndModify();
    mol->ConnectTheDots();
    mol->PerceiveBondOrders();
    
    //qDebug() << "molecule updated";
    return true;
  }
  
  void CartesianEditor::updateCoordinates()
  {
      m_illegalInput = false;
      cartesianEdit->setTextColor(Qt::black);
      QString t = cartesianEdit->toPlainText();
      cartesianEdit->setText(t);

    if (!m_molecule) {
        clear();
    } else {
        QList<Atom*> atomList = m_molecule->atoms();
        QString *coord = new QString;
        QTextStream coordStream(coord);
        coordStream.setRealNumberPrecision(10);

        double k;
        switch (m_unit) {
        case ANGSTROM:
          k=1.0;
          break;
        case BOHR:
          k=ANGSTROM_TO_BOHR;
          break;
        }
        
        for (int i=0; i<atomList.size(); i++) {
          Atom *atom = m_molecule->atom(i);
          switch (m_format) {
          case XYZ:
            coordStream.setFieldWidth(3);
            coordStream << left << QString(OpenBabel::etab.GetSymbol(atom->atomicNumber()));
            coordStream.setFieldWidth(18);
            coordStream << fixed << forcepoint << right << atom->pos()->x()*k << atom->pos()->y()*k
                << atom->pos()->z()*k << endl;
            break;

          case XYZ_NUM:
            coordStream.setFieldWidth(6);
            coordStream << left << QString(OpenBabel::etab.GetSymbol(atom->atomicNumber()))+
            QString::number(i+1);
            coordStream.setFieldWidth(18);
            coordStream << fixed << forcepoint << right << atom->pos()->x()*k << atom->pos()->y()*k
              << atom->pos()->z()*k << endl;
            break;
            
          case GAMESS:
            coordStream.setFieldWidth(3);
            coordStream << left << QString(OpenBabel::etab.GetSymbol(atom->atomicNumber()));
            coordStream.setFieldWidth(3);
            coordStream << right << atom->atomicNumber();
            coordStream.setFieldWidth(2);
            coordStream << left << ".0";
            coordStream.setFieldWidth(18);
            coordStream << fixed << forcepoint << right << atom->pos()->x()*k << atom->pos()->y()*k
              << atom->pos()->z()*k << endl;
            break;

          case GAMESS2:
            coordStream.setFieldWidth(12);
            coordStream << left << QString(OpenBabel::etab.GetName(atom->atomicNumber()).c_str());
            coordStream.setFieldWidth(3);
            coordStream << right << atom->atomicNumber();
            coordStream.setFieldWidth(2);
            coordStream << left << ".0";
            coordStream.setFieldWidth(18);
            coordStream << fixed << forcepoint << right << atom->pos()->x()*k << atom->pos()->y()*k
              << atom->pos()->z()*k << endl;
            break;          

          case TURBOMOLE:
            coordStream.setFieldWidth(14);
            coordStream << fixed << forcepoint << left << right << atom->pos()->x()*k;
            coordStream.setFieldWidth(18);
            coordStream << atom->pos()->y()*k
              << atom->pos()->z()*k;            
            coordStream.setFieldWidth(5);
            coordStream << left << right << QString(OpenBabel::etab.GetSymbol(atom->atomicNumber())) << endl;
            break;

          case PRIRODA:
            coordStream.setFieldWidth(3);
            coordStream << left << atom->atomicNumber();
            coordStream.setFieldWidth(18);
            coordStream << fixed << forcepoint << right << atom->pos()->x()*k << atom->pos()->y()*k
              << atom->pos()->z()*k << endl;
          }
        }
        cartesianEdit->setText(*coord);
        delete coord;
    }
  }
   
  void CartesianEditor::updateAtoms(Atom*)
  {
    updateCoordinates();
  }
 
  void CartesianEditor::moleculeChanged(Molecule *)
  {
    updateCoordinates();
  }

  void CartesianEditor::setMolecule(Molecule *molecule)
  {
    m_molecule = molecule;
    connect(m_molecule, SIGNAL(Avogadro::Primitive::updated()), this, SLOT(updateCoordinates()));
    connect(m_molecule, SIGNAL(atomUpdated(Atom*)), this, SLOT(updateAtoms(Atom*)));
    connect(m_molecule, SIGNAL(atomRemoved(Atom*)), this, SLOT(updateAtoms(Atom*)));
    connect(m_molecule, SIGNAL(moleculeChanged()), this, SLOT(updateCoordinates()));
    updateCoordinates();
  }


  CartesianExtension::CartesianExtension( QObject *parent ) : Extension( parent ), m_molecule(0), m_dialog(0)
  {
    QAction *action;

    action = new QAction( this );
    action->setSeparator(true);
    action->setData(-1);
    m_actions.append(action);
    
    action = new QAction( this );
    action->setText( tr("Cartesian Editor..." ));
    m_actions.append( action );
  }

  CartesianExtension::~CartesianExtension()
  {}

  QList<QAction *> CartesianExtension::actions() const
  {
    return m_actions;
  }

  QString CartesianExtension::menuPath(QAction *action) const
  {
    Q_UNUSED(action)
    return tr("&Build");
  }

  void CartesianExtension::setMolecule(Molecule *molecule)
  {
    if (m_molecule)
      disconnect( m_molecule, 0, this, 0 );

    m_molecule = molecule;
  }

  QUndoCommand* CartesianExtension::performAction(QAction *action,
                                                   GLWidget *widget)
  {
    Q_UNUSED(action)
    QUndoCommand *undo = 0;
        
    if (!m_molecule)
      return 0; // nothing we can do

    // Disconnect in case we're attached to a new widget
    if (m_widget)
      disconnect( m_molecule, 0, this, 0 );

    if (widget)
      m_widget = widget;    

    if (!m_dialog) {
      m_dialog = new CartesianEditor(m_widget);
      m_dialog->setMolecule(m_molecule);      
    }
    
    m_dialog->show();
    m_dialog->updateCoordinates();

    return undo;
  }

  //int OBElementTable::GetAtomicNum(string name, int &iso)
  int GetAtomicNum(string name, int &iso)
  {
    int n = OpenBabel::etab.GetAtomicNum(name.c_str(), iso);
    if (iso != 0)
      return 0;  // "D" ot "T"
    if (n != 0)
      return n;  // other element symbols

    // not match => we've got IUPAC name

    /*vector<OBElement*>::iterator i;
    for (i = _element.begin();i != _element.end();++i)
      if (name == (*i)->GetSymbol())
        return((*i)->GetAtomicNum());*/

    for (unsigned int i=0; i<etab.GetNumberOfElements(); i++)
      if (!QString::compare(name.c_str(), etab.GetName(i).c_str(), Qt::CaseInsensitive))
          return i;
      
    if (!QString::compare(name.c_str(), "Deuterium", Qt::CaseInsensitive)) {
        iso = 2;
        return(1);
    } else if (!QString::compare(name.c_str(), "Tritium", Qt::CaseInsensitive)) {
        iso = 3;
        return(1);
    } else
      iso = 0;
    return(0);
  }

  
} // end namespace Avogadro

Q_EXPORT_PLUGIN2( cartesianextension, Avogadro::CartesianExtensionFactory )

