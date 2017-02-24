/* ----------------------------------------------------------------------
    This is the

    ██╗     ██╗ ██████╗  ██████╗  ██████╗ ██╗  ██╗████████╗███████╗
    ██║     ██║██╔════╝ ██╔════╝ ██╔════╝ ██║  ██║╚══██╔══╝██╔════╝
    ██║     ██║██║  ███╗██║  ███╗██║  ███╗███████║   ██║   ███████╗
    ██║     ██║██║   ██║██║   ██║██║   ██║██╔══██║   ██║   ╚════██║
    ███████╗██║╚██████╔╝╚██████╔╝╚██████╔╝██║  ██║   ██║   ███████║
    ╚══════╝╚═╝ ╚═════╝  ╚═════╝  ╚═════╝ ╚═╝  ╚═╝   ╚═╝   ╚══════╝®

    DEM simulation engine, released by
    DCS Computing Gmbh, Linz, Austria
    http://www.dcs-computing.com, office@dcs-computing.com

    LIGGGHTS® is part of CFDEM®project:
    http://www.liggghts.com | http://www.cfdem.com

    Core developer and main author:
    Christoph Kloss, christoph.kloss@dcs-computing.com

    LIGGGHTS® is open-source, distributed under the terms of the GNU Public
    License, version 2 or later. It is distributed in the hope that it will
    be useful, but WITHOUT ANY WARRANTY; without even the implied warranty
    of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. You should have
    received a copy of the GNU General Public License along with LIGGGHTS®.
    If not, see http://www.gnu.org/licenses . See also top-level README
    and LICENSE files.

    LIGGGHTS® and CFDEM® are registered trade marks of DCS Computing GmbH,
    the producer of the LIGGGHTS® software and the CFDEM®coupling software
    See http://www.cfdem.com/terms-trademark-policy for details.

-------------------------------------------------------------------------
    Contributing author and copyright for this file:

    Richard Berger (JKU Linz)
    Christoph Kloss (DCS Computing GmbH, Linz)
    Christoph Kloss (JKU Linz)

    Copyright 2012-     DCS Computing GmbH, Linz
    Copyright 2009-2012 JKU Linz
------------------------------------------------------------------------- */

#include "comm.h"
#include "error.h"

#include "pair_gran_proxy.h"
#include "granular_pair_style.h"
#include "contact_models.h"

using namespace LAMMPS_NS;
using namespace LIGGGHTS::PairStyles;

PairGranProxy::PairGranProxy(LAMMPS * lmp) : PairGran(lmp), impl(NULL)
{
}

PairGranProxy::~PairGranProxy()
{
  delete impl;
}

void PairGranProxy::settings(int nargs, char ** args)
{
  delete impl;

  //TODO add additional map here which maps tangential "custom" to "history"
  int64_t variant = Factory::instance().selectVariant("gran", nargs, args,force->custom_contact_models);
  impl = Factory::instance().create("gran", variant, lmp, this);

  if(impl) {
    impl->settings(nargs, args, this);
  } else {
    
    error->all(FLERR, "unknown contact model or model not in whitelist. Possible root causes:\n"
                       "  (1) it's a typo. Check the documentation of the contact model you are using.\n"
                       "  (2) the contact model is not available in your installation. Check if a documentation for this.\n"
                       "      contact model is available at all in your version.\n"
                       "  (3) the model is part of a package which was not installed. Check the documentation for details. \n"
                       "  (4) the model is available, but was not in the whitelist during compilation. Check if a file \n"
                       "      src/style_contact_model.whitelist exists. If yes, modify it and re-compile. You can also \n"
                       "      use script whitelist.sh in the /src directory to create a full whitelist.");
  }
}

void PairGranProxy::init_granular()
{
  impl->init_granular();
}

void PairGranProxy::write_restart_settings(FILE * fp)
{
  impl->write_restart_settings(fp);
}

void PairGranProxy::read_restart_settings(FILE * fp)
{
  int me = comm->me;

  int64_t selected = -1, used = -1;
  if(me == 0){
    // read model hashcode, but reset file pointer afterwards.
    // this way read_restart_settings can still read the hashcode (sanity check)
    fread(&selected, sizeof(int64_t), 1, fp);
    fseek(fp, -sizeof(int64_t), SEEK_CUR);
  }
  MPI_Bcast(&selected,8,MPI_CHAR,0,world);

  impl = Factory::instance().create("gran", selected, lmp, this);

  // convert if not found
  if(!impl) {
      const int M = (15) & selected;
      const int T = (15) & selected >> 4;
      const int C = (15) & selected >> 8;
      const int R = (15) & selected >> 12;
      const int S = (15) & selected >> 16;
      error->warning(FLERR, "LIGGGHTS tries to use old-style hashcode to find the contact model. Update your restart file.");
      if(screen) {
          fprintf(screen,"         original hashcode = %zd \n",selected);
          fprintf(screen,"         M = %d, T = %d, C = %d, R = %d, S = %d \n",M,T,C,R,S);
      }
      used = ::LIGGGHTS::ContactModels::generate_gran_hashcode(M,T,C,R,S);
      impl = Factory::instance().create("gran", used, lmp, this);
  }

  if(impl) {
    impl->read_restart_settings(fp, used);
  } else {
    error->one(FLERR, "unknown contact model");
  }
}

void PairGranProxy::compute_force(int eflag, int vflag, int addflag)
{
  impl->compute_force(this, eflag, vflag, addflag);
}

double PairGranProxy::stressStrainExponent()
{
  return impl->stressStrainExponent();
}

int PairGranProxy::get_history_offset(const std::string hname)
{
  return impl->get_history_offset(hname);
}

int64_t PairGranProxy::hashcode() {
  return impl->hashcode();
}

bool PairGranProxy::contact_match(const std::string mtype, const std::string model) {
  return impl->contact_match(mtype, model);
}
