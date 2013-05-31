/*
 * IRC-Dev IRCD - An advanced and innovative IRC Daemon, include/crule.h
 *
 * Copyright (C) 2002-2012 IRC-Dev Development Team <devel@irc-dev.net>
 * Copyright (C) 1990 Darren Reed
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
/** @file crule.h
 * @brief Interfaces and declarations for connection rule checking.
 * @version $Id: crule.h,v 1.7 2007-09-20 21:00:31 zolty Exp $
 */
#ifndef INCLUDED_crule_h
#define INCLUDED_crule_h

/*
 * opaque node pointer
 */
struct CRuleNode;

extern int crule_eval(struct CRuleNode* rule);
extern char *crule_text(struct CRuleNode *rule);

extern struct CRuleNode* crule_make_and(struct CRuleNode *left,
                                        struct CRuleNode *right);
extern struct CRuleNode* crule_make_or(struct CRuleNode *left,
                                       struct CRuleNode *right);
extern struct CRuleNode* crule_make_not(struct CRuleNode *arg);
extern struct CRuleNode* crule_make_connected(char *arg);
extern struct CRuleNode* crule_make_directcon(char *arg);
extern struct CRuleNode* crule_make_via(char *neighbor,
                                        char *server);
extern struct CRuleNode* crule_make_directop(void);
extern void crule_free(struct CRuleNode* elem);

#endif /* INCLUDED_crule_h */
