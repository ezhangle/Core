#include "scroll_bar.h"
#include "scroll_layout.h"

UIScrollBar::UIScrollBar(UIScrollLayout* parent, bool horizontal) : horizontal(horizontal) {

    this->parent = parent;

    bartex.resize(4);

    bartex[0] = texturemanager.grab("ui/layout_tl.png", false);
    bartex[1] = texturemanager.grab("ui/layout_tr.png", false);
    bartex[2] = texturemanager.grab("ui/layout_br.png", false);
    bartex[3] = texturemanager.grab("ui/layout_bl.png", false);

    // NOTE: as the textures are shared this is doing
    // unnessisary work
    foreach(TextureResource* t, bartex) {
        t->bind();
        t->setFiltering(GL_NEAREST, GL_NEAREST);
        t->setWrapStyle(GL_CLAMP);
    }

    inverted = false;

    bar_percent = 0.0f;
    bar_offset  = 0.0f;
    bar_width   = 12.0f;
    bar_min     = 10.0f;
    bar_step    = 0.1f;
    bar_visual_offset = 0.0f;
    
    flip_sides = false;
    dragging   = false;
}

UIScrollBar::~UIScrollBar() {

    foreach(TextureResource* t, bartex) {
        texturemanager.release(t);
    }
    bartex.clear();
}

void UIScrollBar::reset() {
    bar_offset = 0.0f;
}

UIElement* UIScrollBar::elementAt(const vec2& pos) {
    //cant click bar if invisible
    if(bar_percent <= 0.0f) return 0;

    return UIElement::elementAt(pos);
}

void UIScrollBar::flipSides(bool flip_sides) {
    this->flip_sides = flip_sides;    
}

void UIScrollBar::setSelected(bool selected) {
    UIElement::setSelected(selected);
    dragging = false;    
}

void UIScrollBar::idle() {
    dragging = false;
}

void UIScrollBar::drag(const vec2& pos) {

    if(bar_percent <= 0.0f) return;

    float click_offset;
  
    if(horizontal) {
        click_offset = glm::clamp((pos.x - this->pos.x) / rect.x, 0.0f, 1.0f-bar_percent);
    } else {
        click_offset = glm::clamp((pos.y - this->pos.y) / rect.y, 0.0f, 1.0f-bar_percent);
    }  

    if(!dragging) {
        if(click_offset < bar_offset || click_offset > bar_offset + bar_percent) {
            bar_offset = click_offset;
        }

        drag_start = pos;
        dragging   = true;
        return;
    }

    vec2 delta = pos-drag_start;

    drag_start = pos;    
    
    if(click_offset >= bar_offset && click_offset < bar_offset + bar_percent) {
//        return;        
    }
    
    if(horizontal) {
        bar_offset = glm::clamp(bar_offset + (delta.x / rect.x), 0.0f, 1.0f-bar_percent);
    } else {
        bar_offset = glm::clamp(bar_offset + (delta.y / rect.y), 0.0f, 1.0f-bar_percent);
    }
}

void UIScrollBar::mouseWheel(bool up) {
    if(bar_percent <= 0.0f) return;

    float value_inc = bar_step * (1.0f-bar_percent);

    if(up) value_inc = -value_inc;

    Uint8* keyState = SDL_GetKeyState(NULL);

    if(keyState[SDLK_LCTRL]) {
        value_inc *= 0.1f;
    }

    bar_offset = glm::clamp(bar_offset+value_inc, 0.0f, 1.0f-bar_percent);
}

void UIScrollBar::updateRect() {
    if(parent==0) return;

    vec2 scroll_rect = ((UIScrollLayout*)parent)->getScrollRect();
    vec2 inner_rect  = ((UIScrollLayout*)parent)->getInnerRect();

    if(horizontal) {
        rect        = vec2(scroll_rect.x, bar_width);
        bar_percent = std::min(1.0f, scroll_rect.x / inner_rect.x);

        if(bar_percent>= 1.0f) bar_percent = 0.0f;

        bar_rect          = vec2(std::max(bar_min / scroll_rect.x, bar_percent) * rect.x, bar_width);
        bar_visual_offset = std::min(bar_offset, 1.0f - std::max(bar_min / ((UIScrollLayout*)parent)->getScrollRect().x, bar_percent));
    } else {
        rect        = vec2(bar_width, scroll_rect.y);
        bar_percent = std::min(1.0f, scroll_rect.y / inner_rect.y);

        if(bar_percent>= 1.0f) bar_percent = 0.0f;

        bar_rect          = vec2(bar_width, std::max(bar_min / scroll_rect.y, bar_percent) * rect.y);
        bar_visual_offset = std::min(bar_offset, 1.0f - std::max(bar_min / ((UIScrollLayout*)parent)->getScrollRect().y, bar_percent));
    }
}

void UIScrollBar::updatePos() {
    if(parent==0) return;

    vec2 parent_rect = ((UIScrollLayout*)parent)->getRect();

    if(flip_sides) {
        if(horizontal) {
            pos = vec2(parent->pos.x, parent->pos.y-bar_width);
        } else {
            pos = vec2(parent->pos.x-bar_width, parent->pos.y);
        }
    } else {
        if(horizontal) {
            pos = vec2(parent->pos.x, parent->pos.y+parent_rect.y-bar_width);
        } else {
            pos = vec2(parent->pos.x+parent_rect.x-bar_width, parent->pos.y);
        }
    }
}

void UIScrollBar::drawContent() {
    if(bar_percent <= 0.0f) return;

    vec4 texcoord;

    glEnable(GL_TEXTURE_2D);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);

    glPushMatrix();

    if(horizontal) {
        glTranslatef(bar_visual_offset * rect.x, 0.0f, 0.0f);
    } else {
        glTranslatef(0.0f, bar_visual_offset * rect.y, 0.0f);
    }

    //fprintf(stderr, "rect = %.2f, %.2f\n", rect.x, rect.y);

    for(int i=0;i<4;i++) {

        glPushMatrix();

        if(inverted) {
            bartex[(i+2)%4]->bind();

            switch(i) {
                case 0:
                    texcoord = vec4(1.0f, 1.0f, 1.0-(bar_rect.x/32.0f), 1.0-(bar_rect.y/32.0f));
                    break;
                case 1:
                    texcoord = vec4((bar_rect.x/32.0f), 1.0f, 0.0, 1.0-(bar_rect.y/32.0f));
                    glTranslatef(bar_rect.x*0.5, 0.0f, 0.0f);
                    break;
                case 2:
                    texcoord = vec4(bar_rect.x/32.0f, bar_rect.y/32.0f, 0.0f, 0.0f);
                    glTranslatef(bar_rect.x*0.5, bar_rect.y*0.5, 0.0f);
                    break;
                case 3:
                    texcoord = vec4(1.0f, (bar_rect.y/32.0f), 1.0f-(bar_rect.x/32.0f), 0.0f);
                    glTranslatef(0.0f, bar_rect.y*0.5, 0.0f);
                    break;
            }
        } else {
            bartex[i]->bind();

            switch(i) {
                case 0:
                    texcoord = vec4(0.0f, 0.0f, bar_rect.x/32.0f, bar_rect.y/32.0f);
                    break;
                case 1:
                    texcoord = vec4(1.0f-(bar_rect.x/32.0f), 0.0f, 1.0f, (bar_rect.y/32.0f));
                    glTranslatef(bar_rect.x*0.5, 0.0f, 0.0f);
                    break;
                case 2:
                    texcoord = vec4(1.0-bar_rect.x/32.0f, 1.0-bar_rect.y/32.0f, 1.0f, 1.0f);
                    glTranslatef(bar_rect.x*0.5, bar_rect.y*0.5, 0.0f);
                    break;
                case 3:
                    texcoord = vec4(0.0, 1.0-(bar_rect.y/32.0f), (bar_rect.x/32.0f), 1.0f);;
                    glTranslatef(0.0f, bar_rect.y*0.5, 0.0f);
                    break;
            }
        }

            glBegin(GL_QUADS);
                glTexCoord2f(texcoord.x,texcoord.y);
                glVertex2f(0.0f,    0.0f);

                glTexCoord2f(texcoord.z,texcoord.y);
                glVertex2f(bar_rect.x*0.5,  0.0f);

                glTexCoord2f(texcoord.z,texcoord.w);
                glVertex2f(bar_rect.x*0.5, bar_rect.y*0.5);

                glTexCoord2f(texcoord.x,texcoord.w);
                glVertex2f(0.0f,   bar_rect.y*0.5);
            glEnd();

        glPopMatrix();
    }

    glPopMatrix();

}
